#include "ui/game_options.hpp"
#include "options.hpp"
#include <Rocket/Controls.h>

using namespace std;
using namespace Rocket;

extern PandaFramework* framework;

GameOptions::GameOptions(WindowFramework* window, Core::Context* context) : UiBase(window, context)
{
  root = context->LoadDocument("data/options.rml");
  if (root)
  {
    Core::Element* fullscreen_box   = root->GetElementById("fullscreen");
    Core::Element* screen_select    = root->GetElementById("screen-select");
    Core::Element* language_select  = root->GetElementById("language-select");
    vector<string> languages        = i18n::LanguagesAvailable();
    Data           current_language = OptionsManager::Get()["language"];

    {
      stringstream        rml;
      GraphicsPipe*       pipe   = framework->get_default_pipe();
      DisplayInformation* di     = pipe->get_display_information();
      int                 di_tot = di->get_total_display_modes();
      
      for (int i = 0 ; i < di_tot ; ++i)
      {
	rml << "<option value='" << i << "'>";
	rml << di->get_display_mode_width(i) << 'x' << di->get_display_mode_height(i) << ' ';
	if (di->get_display_mode_fullscreen_only(i))
	  rml << " (Fullscreen)";
	rml << "</option>";
      }
      screen_select->SetInnerRML(rml.str().c_str());
    }

    if (OptionsManager::Get()["screen"]["fullscreen"] == 1)
      fullscreen_box->SetAttribute("checked", "on");

    {
      stringstream rml;

      for_each(languages.begin(), languages.end(), [&rml, current_language](string language)
      {
	rml << "<option id='language-" << language << "' value='" << language << "'";
	if (current_language == language)
	  rml << " selected";
	rml << ">" << language << "</option>\n";
      });
      language_select->SetInnerRML(rml.str().c_str());
    }

    {
      Controls::ElementFormControlSelect* select    = dynamic_cast<Controls::ElementFormControlSelect*>(root->GetElementById("language-select"));
      
      for (int i = 0 ; i < select->GetNumOptions() ; ++i)
      {
	Controls::SelectOption* option = select->GetOption(i);
	
    if (option)
	{
	  if (current_language.Value() == option->GetValue().CString())
	  {
	    select->SetSelection(i);
	    break ;
	  }
	}
      }
    }
    
    {
      Controls::ElementFormControlSelect* select = dynamic_cast<Controls::ElementFormControlSelect*>(screen_select);
      
      for (int i = 0 ; i < select->GetNumOptions() ; ++i)
      {
	Controls::SelectOption* option = select->GetOption(i);

    if (option)
	{
	  if (current_language.Value() == option->GetValue().CString())
	  {
	    select->SetSelection(i);
	    break ;
	  }
	}
      }
    }
    
    {
      typedef std::pair<std::string, Controls::ElementFormControlInput*> RadioButton;
      RadioButton radios_yes[] = {
        RadioButton("focus-self",    dynamic_cast<Controls::ElementFormControlInput*>(root->GetElementById("camera-focus"))),
        RadioButton("focus-self",    dynamic_cast<Controls::ElementFormControlInput*>(root->GetElementById("camera-fight-focus"))),
        RadioButton("focus-enemies", dynamic_cast<Controls::ElementFormControlInput*>(root->GetElementById("camera-enemy-focus")))
      };
      RadioButton radios_no[] = {
        RadioButton("focus-self",    dynamic_cast<Controls::ElementFormControlInput*>(root->GetElementById("camera-free"))),
        RadioButton("focus-self",    dynamic_cast<Controls::ElementFormControlInput*>(root->GetElementById("camera-fight-free"))),
        RadioButton("focus-enemies", dynamic_cast<Controls::ElementFormControlInput*>(root->GetElementById("camera-enemy-free")))
      };
      Data        options = OptionsManager::Get();

      for (unsigned int i = 0 ; i < 3 ; ++i)
      {
        RadioButton radio_true  = radios_yes[i];
        RadioButton radio_false = radios_no[i];
        Data        data        = (i == 0 ? options["camera"] : options["camera"]["fight"]);

        if (data[radio_true.first] == 1)
          radio_true.second->Click();
        else
          radio_false.second->Click();
      }
    }

    ToggleEventListener(true, "fullscreen",       "click",  FullscreenToggled);
    ToggleEventListener(true, "screen-select",    "change", ScreenSelected);
    ToggleEventListener(true, "language-select",  "change", LanguageSelected);
    ToggleEventListener(true, "graphics-quality", "change", QualitySelected);
    ToggleEventListener(true, "exit",             "click",  ExitClicked);

    std::string camera_options[] = { "camera-focus", "camera-free", "camera-fight-focus", "camera-fight-free", "camera-enemy-focus", "camera-enemy-free" };
    for (unsigned int i = 0 ; i < 6 ; ++i)
      ToggleEventListener(true, camera_options[i], "click", CameraFocusChanged);

    CameraFocusChanged.EventReceived.Connect([this](Rocket::Core::Event& event)
    {
      Data        options = OptionsManager::Get();
      std::string option  = event.GetCurrentElement()->GetId().CString();

      if (option == "camera-focus")
        options["camera"]["focus-self"] = 1;
      else if (option == "camera-free")
        options["camera"]["focus-self"] = 0;
      else if (option == "camera-fight-focus")
        options["camera"]["fight"]["focus-self"] = 1;
      else if (option == "camera-fight-free")
        options["camera"]["fight"]["focus-self"] = 0;
      else if (option == "camera-enemy-focus")
        options["camera"]["fight"]["focus-enemies"] = 1;
      else if (option == "camera-enemy-free")
        options["camera"]["fight"]["focus-enemies"] = 0;
      OptionsManager::Refresh();
    });

    ExitClicked.EventReceived.Connect      (*this, &UiBase::FireHide);
    FullscreenToggled.EventReceived.Connect(*this, &GameOptions::ToggleFullscreen);
    QualitySelected.EventReceived.Connect  (*this, &GameOptions::SetQuality);
    ScreenSelected.EventReceived.Connect   (*this, &GameOptions::SetResolution);
    LanguageSelected.EventReceived.Connect (*this, &GameOptions::SetLanguage);
    Translate();
  }
}

GameOptions::~GameOptions()
{
  ToggleEventListener(false, "fullscreen",      "click",  FullscreenToggled);
  ToggleEventListener(false, "screen-select",   "change", ScreenSelected);  
  ToggleEventListener(false, "language-select", "change", LanguageSelected);
  ToggleEventListener(false, "graphics-quality", "change", QualitySelected);
  ToggleEventListener(false, "exit",            "click",  ExitClicked);
  std::string camera_options[] = { "camera-focus", "camera-free", "camera-fight-focus", "camera-fight-free", "camera-enemy-focus", "camera-enemy-free" };
  for (unsigned int i = 0 ; i < 6 ; ++i)
    ToggleEventListener(false, camera_options[i], "click", CameraFocusChanged);
  
}

void GameOptions::ToggleFullscreen(Rocket::Core::Event& event)
{
  Core::Element* checkbox = event.GetCurrentElement();
  
  if (checkbox)
  {
    Core::Variant* var  = checkbox->GetAttribute("checked");
    Data           opts = OptionsManager::Get();
    
    opts["screen"]["fullscreen"] = (var ? 1 : 0);
    OptionsManager::Refresh();
  }
}

void GameOptions::SetQuality(Rocket::Core::Event&)
{
  Controls::ElementFormControlSelect* select    = dynamic_cast<Controls::ElementFormControlSelect*>(root->GetElementById("graphics-quality"));
  int                                 it_select = select->GetSelection();
  Controls::SelectOption*             option    = select->GetOption(it_select);
  
  if (option)
  {
    Data        opts = OptionsManager::Get();
    std::string str  = option->GetValue().CString();

    opts["graphics-quality"] = str;
    OptionsManager::Refresh();
  }
}

void GameOptions::SetResolution(Rocket::Core::Event&)
{
  Controls::ElementFormControlSelect* select    = dynamic_cast<Controls::ElementFormControlSelect*>(root->GetElementById("screen-select"));
  int                                 it_select = select->GetSelection();
  Controls::SelectOption*             option    = select->GetOption(it_select);

  if (option)
  {
    stringstream        str_it;
    int                 it;
    Core::String        str    = option->GetValue();
    GraphicsPipe*       pipe   = framework->get_default_pipe();
    DisplayInformation* di     = pipe->get_display_information();
    
    str_it << str.CString();
    str_it >> it;
    if (it < di->get_total_display_modes())
    {
      Data opts = OptionsManager::Get();
      
      opts["screen"]["x"] = di->get_display_mode_width(it);
      opts["screen"]["y"] = di->get_display_mode_height(it);
      OptionsManager::Refresh();
    }
  }
}

void GameOptions::SetLanguage(Core::Event& event)
{
  Controls::ElementFormControlSelect* select    = dynamic_cast<Controls::ElementFormControlSelect*>(root->GetElementById("language-select"));
  int                                 it_select = select->GetSelection();
  Controls::SelectOption*             option    = select->GetOption(it_select);
  
  if (option)
  {
    Core::String str  = option->GetValue();
    Data         opts = OptionsManager::Get();

    opts["language"] = str.CString();
    OptionsManager::Refresh();    
  }
}

