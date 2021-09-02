/* SystemEditor.cpp
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "SystemEditor.h"

#include "DataFile.h"
#include "DataWriter.h"
#include "Editor.h"
#include "imgui.h"
#include "imgui_ex.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"
#include "GameData.h"
#include "Government.h"
#include "Hazard.h"
#include "MainEditorPanel.h"
#include "MapPanel.h"
#include "MapEditorPanel.h"
#include "Minable.h"
#include "Planet.h"
#include "PlayerInfo.h"
#include "SpriteSet.h"
#include "Sprite.h"
#include "System.h"
#include "UI.h"
#include "Visual.h"

using namespace std;



SystemEditor::SystemEditor(Editor &editor, bool &show) noexcept
	: TemplateEditor<System>(editor, show)
{
}



void SystemEditor::UpdateSystemPosition(const System *system, Point dp)
{
	const_cast<System *>(system)->position += dp;
	SetDirty(system);
}



void SystemEditor::UpdateStellarPosition(const StellarObject &object, Point dp, const System *system)
{
	auto &obj = const_cast<StellarObject &>(object);
	double now = editor.Player().GetDate().DaysSinceEpoch();

	auto newPos = obj.position + dp;
	if(obj.parent != -1)
		newPos -= this->object->objects[obj.parent].position;

	obj.distance = newPos.Length();
	Angle newAngle(newPos);

	obj.speed = (newAngle.Degrees() - obj.offset) / now;
	const_cast<System *>(system)->SetDate(editor.Player().GetDate());

	SetDirty(this->object);
}



void SystemEditor::Render()
{
	if(IsDirty())
	{
		ImGui::PushStyleColor(ImGuiCol_TitleBg, static_cast<ImVec4>(ImColor(255, 91, 71)));
		ImGui::PushStyleColor(ImGuiCol_TitleBgActive, static_cast<ImVec4>(ImColor(255, 91, 71)));
		ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, static_cast<ImVec4>(ImColor(255, 91, 71)));
	}

	ImGui::SetNextWindowSize(ImVec2(550, 500), ImGuiCond_FirstUseEver);
	if(!ImGui::Begin("System Editor", &show))
	{
		if(IsDirty())
			ImGui::PopStyleColor(3);
		ImGui::End();
		return;
	}

	if(IsDirty())
		ImGui::PopStyleColor(3);

	if(auto *panel = dynamic_cast<MapEditorPanel*>(editor.GetMenu().Top().get()))
		object = const_cast<System *>(panel->Selected());
	if(auto *panel = dynamic_cast<MainEditorPanel*>(editor.GetMenu().Top().get()))
		object = const_cast<System *>(panel->Selected());

	System *selected = nullptr;
	if(ImGui::InputCombo("system", &searchBox, &selected, GameData::Systems()))
		if(selected)
		{
			searchBox.clear();
			if(auto *panel = dynamic_cast<MapEditorPanel*>(editor.GetMenu().Top().get()))
				panel->Select(object);
			if(auto *panel = dynamic_cast<MainEditorPanel*>(editor.GetMenu().Top().get()))
				panel->Select(object);
		}
	if(!object || !IsDirty())
		ImGui::PushDisabled();
	bool reset = ImGui::Button("Reset");
	if(!object || !IsDirty())
	{
		ImGui::PopDisabled();
		if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			if(!object)
				ImGui::SetTooltip("Select a system first.");
			else if(!IsDirty())
				ImGui::SetTooltip("No changes to reset.");
		}
	}
	ImGui::SameLine();
	if(!object || searchBox.empty())
		ImGui::PushDisabled();
	bool clone = ImGui::Button("Clone");
	if(!object || searchBox.empty())
	{
		ImGui::PopDisabled();
		if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			if(searchBox.empty())
				ImGui::SetTooltip("Input the new name for the system above.");
			else if(!object)
				ImGui::SetTooltip("Select a system first.");
		}
	}
	ImGui::SameLine();
	if(!object || !editor.HasPlugin() || !IsDirty())
		ImGui::PushDisabled();
	bool save = ImGui::Button("Save");
	if(!object || !editor.HasPlugin() || !IsDirty())
	{
		ImGui::PopDisabled();
		if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			if(!object)
				ImGui::SetTooltip("Select a system first.");
			else if(!editor.HasPlugin())
				ImGui::SetTooltip("Load a plugin to save to a file.");
			else if(!IsDirty())
				ImGui::SetTooltip("No changes to save.");
		}
	}

	if(!object)
	{
		ImGui::End();
		return;
	}

	if(reset)
	{
		bool found = false;
		for(auto &&change : Changes())
			if(change.Name() == object->Name())
			{
				*object = change;
				found = true;
				break;
			}
		if(!found)
			*object = *GameData::baseSystems.Get(object->name);
		for(auto &&link : object->links)
			const_cast<System *>(link)->Link(object);
		UpdateMap();
		SetClean();
	} 
	if(clone)
	{
		auto *clone = const_cast<System *>(GameData::Systems().Get(searchBox));
		*clone = *object;
		object = clone;

		object->name = searchBox;
		object->position += Point(25., 25.);
		object->objects.clear();
		object->links.clear();
		object->attributes.insert("uninhabited");
		GameData::UpdateSystems();
		UpdateMap(/*updateSystem=*/false);
		searchBox.clear();
		SetDirty();
	}
	if(save)
		WriteToPlugin(object);

	ImGui::Separator();
	ImGui::Spacing();
	RenderSystem();
	ImGui::End();
}



void SystemEditor::RenderSystem()
{
	int index = 0;

	ImGui::Text("name: %s", object->name.c_str());
	if(ImGui::Checkbox("hidden", &object->hidden))
		SetDirty();

	if(ImGui::TreeNode("attributes"))
	{
		set<string> toAdd;
		set<string> toRemove;
		for(auto &attribute : object->attributes)
		{
			if(attribute == "uninhabited")
				continue;

			ImGui::PushID(index++);
			string str = attribute;
			if(ImGui::InputText("", &str, ImGuiInputTextFlags_EnterReturnsTrue))
			{
				if(!str.empty())
					toAdd.insert(move(str));
				toRemove.insert(attribute);
			}
			ImGui::PopID();
		}
		for(auto &&attribute : toAdd)
			object->attributes.insert(attribute);
		for(auto &&attribute : toRemove)
			object->attributes.erase(attribute);
		if(!toAdd.empty() || !toRemove.empty())
			SetDirty();

		ImGui::Spacing();

		static string addAttribute;
		if(ImGui::InputText("##system", &addAttribute, ImGuiInputTextFlags_EnterReturnsTrue))
		{
			object->attributes.insert(move(addAttribute));
			SetDirty();
		}
		ImGui::TreePop();
	}

	if(ImGui::TreeNode("links"))
	{
		set<System *> toAdd;
		set<System *> toRemove;
		index = 0;
		for(auto &link : object->links)
		{
			ImGui::PushID(index++);
			static System *newLink = nullptr;
			string name = link->Name();
			if(ImGui::InputCombo("link", &name, &newLink, GameData::Systems()))
			{
				if(newLink)
					toAdd.insert(newLink);
				newLink = nullptr;
				toRemove.insert(const_cast<System *>(link));
			}
			ImGui::PopID();
		}
		static System *newLink = nullptr;
		string addLink;
		if(ImGui::InputCombo("add link", &addLink, &newLink, GameData::Systems()))
		{
			toAdd.insert(newLink);
			newLink = nullptr;
		}

		for(auto &sys : toAdd)
		{
			object->Link(sys);
			SetDirty(sys);
		}
		for(auto &&sys : toRemove)
		{
			object->Unlink(sys);
			SetDirty(sys);
		}
		if(!toAdd.empty() || !toRemove.empty())
		{
			if(!toAdd.empty())
				editor.Player().Seen(*object);
			SetDirty();
			UpdateMap();
		}
		ImGui::TreePop();
	}

	bool asteroidsOpen = ImGui::TreeNode("asteroids");
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add Asteroid"))
		{
			object->asteroids.emplace_back("small rock", 1, 1.);
			if(auto *panel = dynamic_cast<MainEditorPanel*>(editor.GetMenu().Top().get()))
				panel->UpdateCache();
			SetDirty();
		}
		if(ImGui::Selectable("Add Mineable"))
		{
			object->asteroids.emplace_back(&GameData::Minables().begin()->second, 1, 1.);
			if(auto *panel = dynamic_cast<MainEditorPanel*>(editor.GetMenu().Top().get()))
				panel->UpdateCache();
			SetDirty();
		}
		ImGui::EndPopup();
	}

	if(asteroidsOpen)
	{
		index = 0;
		int toRemove = -1;
		for(auto &asteroid : object->asteroids)
		{
			ImGui::PushID(index);
			if(asteroid.Type())
			{
				bool open = ImGui::TreeNode("minables", "mineables: %s %d %g", asteroid.Type()->Name().c_str(), asteroid.count, asteroid.energy);
				if(ImGui::BeginPopupContextItem())
				{
					if(ImGui::Selectable("Remove"))
						toRemove = index;
					ImGui::EndPopup();
				}

				if(open)
				{
					if(ImGui::BeginCombo("name", asteroid.Type()->Name().c_str()))
					{
						int index = 0;
						for(const auto &item : GameData::Minables())
						{
							const bool selected = &item.second == asteroid.Type();
							if(ImGui::Selectable(item.first.c_str(), selected))
							{
								asteroid.type = &item.second;
								if(auto *panel = dynamic_cast<MainEditorPanel*>(editor.GetMenu().Top().get()))
									panel->UpdateCache();
								SetDirty();
							}
							++index;

							if(selected)
								ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}
					if(ImGui::InputInt("count", &asteroid.count))
					{
						if(auto *panel = dynamic_cast<MainEditorPanel*>(editor.GetMenu().Top().get()))
							panel->UpdateCache();
						SetDirty();
					}
					if(ImGui::InputDoubleEx("energy", &asteroid.energy))
					{
						if(auto *panel = dynamic_cast<MainEditorPanel*>(editor.GetMenu().Top().get()))
							panel->UpdateCache();
						SetDirty();
					}
					ImGui::TreePop();
				}
			}
			else
			{
				bool open = ImGui::TreeNode("asteroids", "asteroids: %s %d %g", asteroid.name.c_str(), asteroid.count, asteroid.energy);
				if(ImGui::BeginPopupContextItem())
				{
					if(ImGui::Selectable("Remove"))
					{
						if(auto *panel = dynamic_cast<MainEditorPanel*>(editor.GetMenu().Top().get()))
							panel->UpdateCache();
						toRemove = index;
					}
					ImGui::EndPopup();
				}

				if(open)
				{
					if(ImGui::InputText("name", &asteroid.name))
					{
						if(auto *panel = dynamic_cast<MainEditorPanel*>(editor.GetMenu().Top().get()))
							panel->UpdateCache();
						SetDirty();
					}
					if(ImGui::InputInt("count", &asteroid.count))
					{
						if(auto *panel = dynamic_cast<MainEditorPanel*>(editor.GetMenu().Top().get()))
							panel->UpdateCache();
						SetDirty();
					}
					if(ImGui::InputDoubleEx("energy", &asteroid.energy))
					{
						if(auto *panel = dynamic_cast<MainEditorPanel*>(editor.GetMenu().Top().get()))
							panel->UpdateCache();
						SetDirty();
					}
					ImGui::TreePop();
				}
			}
			++index;
			ImGui::PopID();
		}

		if(toRemove != -1)
		{
			object->asteroids.erase(object->asteroids.begin() + toRemove);
			if(auto *panel = dynamic_cast<MainEditorPanel*>(editor.GetMenu().Top().get()))
				panel->UpdateCache();
			SetDirty();
		}
		ImGui::TreePop();
	}

	bool fleetOpen = ImGui::TreeNode("fleets");
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add Fleet"))
			object->fleets.emplace_back(&GameData::Fleets().begin()->second, 1);
		ImGui::EndPopup();
	}

	if(fleetOpen)
	{
		index = 0;
		int toRemove = -1;
		for(auto &fleet : object->fleets)
		{
			ImGui::PushID(index);
			bool open = ImGui::TreeNode("fleet", "fleet: %s %d", fleet.Get()->Name().c_str(), fleet.period);
			if(ImGui::BeginPopupContextItem())
			{
				if(ImGui::Selectable("Remove"))
					toRemove = index;
				ImGui::EndPopup();
			}

			if(open)
			{
				if(ImGui::BeginCombo("fleet", fleet.Get()->Name().c_str()))
				{
					int index = 0;
					for(const auto &item : GameData::Fleets())
					{
						const bool selected = &item.second == fleet.Get();
						if(ImGui::Selectable(item.first.c_str(), selected))
						{
							fleet.fleet = &item.second;
							SetDirty();
						}
						++index;

						if(selected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
				if(ImGui::InputInt("period", &fleet.period))
					SetDirty();
				ImGui::TreePop();
			}
			++index;
			ImGui::PopID();
		}
		if(toRemove != -1)
		{
			object->fleets.erase(object->fleets.begin() + toRemove);
			SetDirty();
		}
		ImGui::TreePop();
	}

	bool hazardOpen = ImGui::TreeNode("hazards");
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add Hazard"))
			object->hazards.emplace_back(&GameData::Hazards().begin()->second, 1);
		ImGui::EndPopup();
	}

	if(hazardOpen)
	{
		index = 0;
		int toRemove = -1;
		for(auto &hazard : object->hazards)
		{
			ImGui::PushID(index);
			bool open = ImGui::TreeNode("hazard", "hazard: %s %d", hazard.Get()->Name().c_str(), hazard.period);
			if(ImGui::BeginPopupContextItem())
			{
				if(ImGui::Selectable("Remove"))
					toRemove = index;
				ImGui::EndPopup();
			}

			if(open)
			{
				if(ImGui::BeginCombo("hazard", hazard.Get()->Name().c_str()))
				{
					int index = 0;
					for(const auto &item : GameData::Hazards())
					{
						const bool selected = &item.second == hazard.Get();
						if(ImGui::Selectable(item.first.c_str(), selected))
						{
							hazard.hazard = &item.second;
							SetDirty();
						}
						++index;

						if(selected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
				if(ImGui::InputInt("period", &hazard.period))
					SetDirty();
				ImGui::TreePop();
			}
			++index;
			ImGui::PopID();
		}

		if(toRemove != -1)
		{
			object->hazards.erase(object->hazards.begin() + toRemove);
			SetDirty();
		}
		ImGui::TreePop();
	}

	double pos[2] = {object->position.X(), object->Position().Y()};
	if(ImGui::InputDouble2Ex("pos", pos, ImGuiInputTextFlags_EnterReturnsTrue))
	{
		object->position.Set(pos[0], pos[1]);
		SetDirty();
	}

	{
		if(ImGui::BeginCombo("government", object->government ? object->government->TrueName().c_str() : ""))
		{
			for(const auto &government : GameData::Governments())
			{
				const bool selected = &government.second == object->government;
				if(ImGui::Selectable(government.first.c_str(), selected))
				{
					object->government = &government.second;
					UpdateMap(/*updateSystems=*/false);
					SetDirty();
				}

				if(selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
	}

	if(ImGui::InputText("music", &object->music))
		SetDirty();

	if(ImGui::InputDoubleEx("habitable", &object->habitable))
		SetDirty();
	if(ImGui::InputDoubleEx("belt", &object->asteroidBelt))
		SetDirty();
	if(ImGui::InputDoubleEx("jump range", &object->jumpRange))
		SetDirty();
	if(object->jumpRange < 0.)
		object->jumpRange = 0.;
	string enterHaze = object->haze ? object->haze->Name() : "";
	if(ImGui::InputCombo("haze", &enterHaze, &object->haze, SpriteSet::GetSprites()))
	{
		enterHaze = object->haze->Name();
		SetDirty();
	}

	double arrival[2] = {object->extraHyperArrivalDistance, object->extraJumpArrivalDistance};
	if(ImGui::InputDouble2Ex("arrival", arrival))
		SetDirty();
	object->extraHyperArrivalDistance = arrival[0];
	object->extraJumpArrivalDistance = fabs(arrival[1]);

	if(ImGui::TreeNode("trades"))
	{
		index = 0;
		for(auto &&commodity : GameData::Commodities())
		{
			ImGui::PushID(index++);
			ImGui::Text("trade: %s", commodity.name.c_str());
			ImGui::SameLine();
			if(ImGui::InputInt("", &object->trade[commodity.name].base))
				SetDirty();
			ImGui::PopID();
		}
		ImGui::TreePop();
	}

	bool openObjects = ImGui::TreeNode("objects");
	bool openAddObject = false;
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add Object"))
			openAddObject = true;
		ImGui::EndPopup();
	}
	if(openAddObject)
	{
		object->objects.emplace_back();
		SetDirty();
	}

	if(openObjects)
	{
		bool hovered = false;
		bool add = false;
		index = 0;
		int nested = 0;
		auto selected = object->objects.end();
		auto selectedToAdd = object->objects.end();
		for(auto it = object->objects.begin(); it != object->objects.end(); ++it)
		{
			ImGui::PushID(index);
			RenderObject(*it, index, nested, hovered, add);
			if(hovered)
			{
				selected = it;
				hovered = false;
			}
			if(add)
			{
				selectedToAdd = it;
				add = false;
			}
			++index;
			ImGui::PopID();
		}
		ImGui::TreePop();

		if(selected != object->objects.end())
		{
			if(auto *planet = selected->GetPlanet())
				const_cast<Planet *>(planet)->RemoveSystem(object);
			SetDirty();
			auto index = selected - object->objects.begin();
			auto next = object->objects.erase(selected);
			size_t removed = 1;
			// Remove any child objects too.
			while(next != object->objects.end() && next->Parent() == index)
			{
				next = object->objects.erase(next);
				++removed;
			}

			// Recalculate every parent index.
			for(auto it = next; it != object->objects.end(); ++it)
				if(it->Parent() != -1)
					it->parent -= removed;
		}
		else if(selectedToAdd != object->objects.end())
		{
			SetDirty();
			auto it = object->objects.emplace(selectedToAdd + 1);
			it->parent = selectedToAdd - object->objects.begin();

			int newParent = it->parent;
			for(++it; it != object->objects.end(); ++it)
				if(it->parent >= newParent)
					++it->parent;
		}
	}
}



void SystemEditor::RenderObject(StellarObject &object, int index, int &nested, bool &hovered, bool &add)
{
	if(object.parent != -1 && !nested)
		return;

	bool isOpen = ImGui::TreeNode("object", "object %s", object.GetPlanet() ? object.GetPlanet()->TrueName().c_str() : "");

	ImGui::PushID(index);
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::MenuItem("Add Child"))
			add = true;
		if(ImGui::MenuItem("Remove"))
			hovered = true;
		ImGui::EndPopup();
	}
	ImGui::PopID();

	if(isOpen)
	{
		static Planet *planet = nullptr;
		static string planetName;
		planetName.clear();
		if(object.planet)
			planetName = object.planet->TrueName();
		if(ImGui::InputCombo("planet", &planetName, &planet, GameData::Planets()))
		{
			if(object.planet)
				const_cast<Planet *>(object.planet)->RemoveSystem(object.planet->GetSystem());
			object.planet = planet;
			planet->SetSystem(this->object);
			planet = nullptr;
			SetDirty();
		}
		static Sprite *sprite = nullptr;
		static string spriteName;
		spriteName.clear();
		if(object.sprite)
			spriteName = object.sprite->Name();
		if(ImGui::InputCombo("sprite", &spriteName, &sprite, SpriteSet::GetSprites()))
		{
			object.sprite = SpriteSet::Get(spriteName);
			sprite = nullptr;
			SetDirty();
		}

		if(ImGui::InputDoubleEx("distance", &object.distance))
			SetDirty();
		double period = 0.;
		if(object.Speed())
			period = 360. / object.Speed();
		if(ImGui::InputDoubleEx("period", &period))
			SetDirty();
		object.speed = 360. / period;
		if(ImGui::InputDoubleEx("offset", &object.offset))
			SetDirty();

		if(IsDirty())
			this->object->SetDate(editor.Player().GetDate());

		if(index + 1 < static_cast<int>(this->object->objects.size()) && this->object->objects[index + 1].Parent() == index)
		{
			++nested; // If the next object is a child, don't close this tree node just yet.
			return;
		}
		else
			ImGui::TreePop();
	}

	// If are nested, then we need to remove this nesting until we are at the next desired level.
	if(nested && index + 1 >= static_cast<int>(this->object->objects.size()))
		while(nested--)
			ImGui::TreePop();
	else if(nested)
	{
		int nextParent = this->object->objects[index + 1].Parent();
		if(nextParent == object.Parent())
			return;
		while(nextParent != index)
		{
			nextParent = nextParent == -1 ? index : this->object->objects[nextParent].Parent();
			--nested;
			ImGui::TreePop();
		}
	}
}



void SystemEditor::WriteObject(DataWriter &writer, const System *system, const StellarObject *object, bool add)
{
	// Calculate the nesting of this object. We follow parent indices until we reach
	// the root node.
	int i = object->Parent();
	int nested = 0;
	while(i != -1)
	{
		i = system->objects[i].Parent();
		++nested;
	}

	for(i = 0; i < nested; ++i)
		writer.BeginChild();

	if(add && !nested)
		writer.WriteToken("add");
	writer.WriteToken("object");

	if(object->GetPlanet())
		writer.WriteToken(object->GetPlanet()->TrueName());
	writer.Write();

	writer.BeginChild();
	if(object->GetSprite())
		writer.Write("sprite", object->GetSprite()->Name());
	if(object->distance)
		writer.Write("distance", object->Distance());
	if(object->speed)
		writer.Write("period", 360. / object->Speed());
	if(object->Offset())
		writer.Write("offset", object->Offset());
	writer.EndChild();

	for(i = 0; i < nested; ++i)
		writer.EndChild();
}



void SystemEditor::WriteToFile(DataWriter &writer, const System *system)
{
	const auto *diff = GameData::baseSystems.Has(system->name)
		? GameData::baseSystems.Get(system->name)
		: nullptr;

	writer.Write("system", system->name);
	writer.BeginChild();

	if((!diff && system->hasPosition) || (diff && (system->hasPosition != diff->hasPosition || system->position != diff->position)))
		writer.Write("pos", system->position.X(), system->position.Y());
	if(!diff || system->government != diff->government)
	{
		if(system->government)
			writer.Write("government", system->government->TrueName());
		else if(diff)
			writer.Write("remove", "government");
	}
	if(!diff || system->music != diff->music)
	{
		if(!system->music.empty())
			writer.Write("music", system->music);
		else if(diff)
			writer.Write("remove", "music");
	}
	WriteDiff(writer, "link", system->links, diff ? &diff->links : nullptr);
	if(!diff || system->hidden != diff->hidden)
	{
		if(system->hidden)
			writer.Write("hidden");
		else if(diff)
			writer.Write("remove hidden");
	}

	auto asteroids = system->asteroids;
	asteroids.erase(std::remove_if(asteroids.begin(), asteroids.end(), [](const System::Asteroid &a) { return a.Type(); }), asteroids.end());
	auto minables = system->asteroids;
	minables.erase(std::remove_if(minables.begin(), minables.end(), [](const System::Asteroid &a) { return !a.Type(); }), minables.end());
	auto diffAsteroids = diff ? diff->asteroids : system->asteroids;
	diffAsteroids.erase(std::remove_if(diffAsteroids.begin(), diffAsteroids.end(), [](const System::Asteroid &a) { return a.Type(); }), diffAsteroids.end());
	auto diffMinables = diff ? diff->asteroids : system->asteroids;
	diffMinables.erase(std::remove_if(diffMinables.begin(), diffMinables.end(), [](const System::Asteroid &a) { return !a.Type(); }), diffMinables.end());
	WriteDiff(writer, "asteroids", asteroids, diff ? &diffAsteroids : nullptr);
	WriteDiff(writer, "minables", minables, diff ? &diffMinables : nullptr);

	if(!diff || system->haze != diff->haze)
	{
		if(system->haze)
			writer.Write("haze", system->haze->Name());
		else if(diff)
			writer.Write("remove", "haze");
	}
	WriteDiff(writer, "fleet", system->fleets, diff ? &diff->fleets : nullptr);
	WriteDiff(writer, "hazard", system->hazards, diff ? &diff->hazards : nullptr);
	if((!diff && system->habitable != 1000.) || (diff && system->habitable != diff->habitable))
		writer.Write("habitable", system->habitable);
	if((!diff && system->asteroidBelt != 1500.) || (diff && system->asteroidBelt != diff->asteroidBelt))
		writer.Write("belt", system->asteroidBelt);
	if((!diff && system->jumpRange)|| (diff && system->jumpRange != diff->jumpRange))
		writer.Write("jump range", system->jumpRange);
	if(!diff || system->extraHyperArrivalDistance != diff->extraHyperArrivalDistance
			|| system->extraJumpArrivalDistance != diff->extraJumpArrivalDistance)
	{
		if(system->extraHyperArrivalDistance == system->extraJumpArrivalDistance
				&& (diff || system->extraHyperArrivalDistance))
			writer.Write("arrival", system->extraHyperArrivalDistance);
		else if(system->extraHyperArrivalDistance != system->extraJumpArrivalDistance)
		{
			writer.Write("arrival");
			writer.BeginChild();
			if((!diff && system->extraHyperArrivalDistance) || system->extraHyperArrivalDistance != diff->extraHyperArrivalDistance)
				writer.Write("link", system->extraHyperArrivalDistance);
			if((!diff && system->extraJumpArrivalDistance) || system->extraJumpArrivalDistance != diff->extraJumpArrivalDistance)
				writer.Write("jump", system->extraJumpArrivalDistance);
			writer.EndChild();
		}
	}
	if(!diff || system->trade != diff->trade)
	{
		if(!system->trade.empty())
			for(auto &&trade : system->trade)
				writer.Write("trade", trade.first, trade.second.base);
		else if(diff)
			writer.Write("remove", "trade");
	}

	auto systemAttributes = system->attributes;
	auto diffAttributes = diff ? diff->attributes : system->attributes;
	systemAttributes.erase("uninhabited");
	diffAttributes.erase("uninhabited");
	WriteDiff(writer, "attributes", systemAttributes, diff ? &diffAttributes : nullptr, true);

	if(!diff || system->objects != diff->objects)
	{
		std::vector<const StellarObject *> toAdd;
		if(diff && system->objects.size() > diff->objects.size())
		{
			std::transform(system->objects.begin() + diff->objects.size(), system->objects.end(),
					back_inserter(toAdd), [](const StellarObject &obj) { return &obj; });

			for(int i = 0; i < static_cast<int>(diff->objects.size()); ++i)
				if(system->objects[i] != diff->objects[i])
				{
					toAdd.clear();
					break;
				}
		}

		if(!toAdd.empty())
			for(auto &&object : toAdd)
				WriteObject(writer, system, object, true);
		else if(!system->objects.empty())
			for(auto &&object : system->objects)
				WriteObject(writer, system, &object);
		else if(diff)
			writer.Write("remove object");
	}

	writer.EndChild();
}



void SystemEditor::UpdateMap(bool updateSystem) const
{
	if(updateSystem)
		GameData::UpdateSystems();
	if(auto *mapPanel = dynamic_cast<MapPanel*>(editor.GetUI().Top().get()))
	{
		mapPanel->UpdateCache();
		mapPanel->distance = DistanceMap(editor.Player());
	}
	if(auto *panel = dynamic_cast<MapEditorPanel*>(editor.GetMenu().Top().get()))
		panel->UpdateCache();
}
