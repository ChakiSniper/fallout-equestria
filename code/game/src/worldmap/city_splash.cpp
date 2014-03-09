#include "worldmap/city_splash.hpp"
#include <sstream>

using namespace std;

CitySplash::CitySplash(Data data, WindowFramework* window, Rocket::Core::Context* context) : UiBase(window, context)
{
  root = context->LoadDocument("data/city_splash.rml");
  if (!root)
    throw 0;

  Rocket::Core::Element* splash      = root->GetElementById("splash");
  Data                   entry_zones = data["zones"];
  Rocket::Core::Element* zoneroot   = root->GetElementById("zones");

  if (splash)
  {
    string path = "../textures/" + data["splashscreen"].Value();

    splash->SetAttribute("src", path.c_str());
  }
  if (zoneroot)
  {
    for_each(entry_zones.begin(), entry_zones.end(), [this, zoneroot](Data zone)
    {
      if (zone == '1')
      {
        Rocket::Core::String rml;
        stringstream         stream;
        std::string          zone_name = "zone-" + underscore(zone.Key());

        stream << "<div class='zone-button' id='" << zone_name << "'>";
        stream << zone.Key();
        stream << "</div>";
        zoneroot->GetInnerRML(rml);
        rml += stream.str().c_str();
        zoneroot->SetInnerRML(rml);
        {
          RocketListener* listener = new RocketListener;

          ToggleEventListener(true, zone_name, "click", *listener);
          listener->EventReceived.Connect(*this, &CitySplash::ZonePicked);
          zone_listeners.insert(CitySplash::ZoneListeners::value_type(zone.Key(), listener));
        }
      }
    });
  }
  ToggleEventListener(true, "button-cancel", "click", CancelClicked);
  CancelClicked.EventReceived.Connect([this](Rocket::Core::Event&)
  {
    Canceled.Emit();
  });
}

CitySplash::~CitySplash()
{
  for_each(zone_listeners.begin(), zone_listeners.end(), [this](ZoneListeners::value_type listener)
  {
    string name = "zone-" + humanize(listener.first);

    ToggleEventListener(false, name, "click", *listener.second);
  });
}

void CitySplash::ZonePicked(Rocket::Core::Event& event)
{
  Rocket::Core::Element*  element   = event.GetCurrentElement();
  std::string             zone_name = element->GetId().CString();
  ZoneListeners::iterator item;

  zone_name = zone_name.substr(5);
  item      = zone_listeners.find(zone_name);
  if (item != zone_listeners.end())
    EntryZonePicked.Emit(zone_name);
}
