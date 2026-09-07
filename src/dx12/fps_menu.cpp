#include "fps_menu.h"
#include <windows.h>
#include <imgui/imgui.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>
#include <map>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace mxl::dx12 {
static std::filesystem::path config_path() {
    wchar_t exe[32768]{};if(!GetModuleFileNameW(nullptr,exe,32768))throw std::runtime_error("Cannot locate the game folder.");
    return std::filesystem::path(exe).parent_path()/L"d2fps.ini";
}
static std::string read_config() {
    std::ifstream file(config_path(),std::ios::binary);std::ostringstream result;result<<file.rdbuf();return result.str();
}
static std::map<std::string,std::string> values(const std::string& text) {
    std::map<std::string,std::string> result;std::istringstream input(text);std::string line;
    const std::regex setting(R"(^\s*([A-Za-z0-9-]+)\s*=\s*([^;\r\n]*))");
    while(std::getline(input,line)){std::smatch m;if(std::regex_search(line,m,setting)){auto value=m[2].str();while(!value.empty()&&isspace(static_cast<unsigned char>(value.back())))value.pop_back();result[m[1].str()]=value;}}
    return result;
}
static bool valid_fps(const char* input) {
    if(!input||!*input)return false;char* end=nullptr;double numerator=std::strtod(input,&end);
    if(end==input||!std::isfinite(numerator)||numerator<0)return false;
    if(*end=='/'){char* last=nullptr;double denominator=std::strtod(end+1,&last);return last!=end+1&&!*last&&std::isfinite(denominator)&&denominator>0;}
    return !*end;
}
static void save_values(const std::map<std::string,std::string>& changed) {
    auto path=config_path();auto original=read_config();auto remaining=changed;
    const auto eol=original.find("\r\n")==std::string::npos?"\n":"\r\n";
    std::istringstream input(original);std::ostringstream result;std::string line;std::map<std::string,bool> seen;
    const std::regex setting(R"(^\s*([A-Za-z0-9-]+)\s*=)");
    while(std::getline(input,line)){
        if(!line.empty()&&line.back()=='\r')line.pop_back();std::smatch m;
        if(std::regex_search(line,m,setting)){
            auto it=changed.find(m[1].str());
            if(it!=changed.end()){
                if(seen[it->first])throw std::runtime_error("The INI has duplicate settings. Edit those lines first.");
                seen[it->first]=true;line=it->first+"="+it->second;remaining.erase(it->first);
            }
        }
        result<<line<<eol;
    }
    for(auto& [key,value]:remaining)result<<key<<"="<<value<<eol;
    if(std::filesystem::exists(path)){
        auto backup=path;backup+=L".before-dx12-menu";
        if(!std::filesystem::exists(backup))std::filesystem::copy_file(path,backup);
    }
    auto temporary=path;temporary+=L".dx12-tmp";
    {std::ofstream out(temporary,std::ios::binary|std::ios::trunc);const auto text=result.str();out.write(text.data(),text.size());out.flush();if(!out)throw std::runtime_error("Could not save the FPS settings.");}
    if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Could not replace d2fps.ini.");
}
void draw_fps_settings() {
    static bool loaded=false,game=true,menu=true,smoothing=true,weather=true,animations=true;
    static char fps[48]="0",background[48]="25";
    static std::string message;
    if(!loaded){
        try{
            auto v=values(read_config());
            if(v.count("fps"))strncpy_s(fps,v["fps"].c_str(),_TRUNCATE);
            if(v.count("bg-fps"))strncpy_s(background,v["bg-fps"].c_str(),_TRUNCATE);
            auto get=[&](const char* name,bool fallback){auto it=v.find(name);return it==v.end()?fallback:it->second=="true";};
            game=get("game-fps",true);menu=get("menu-fps",true);smoothing=get("motion-smoothing",true);
            weather=get("weather-smoothing",true);animations=get("anim-rate-fixes",true);
        }catch(const std::exception& e){message=e.what();}
        loaded=true;
    }
    ImGui::TextUnformatted("D2FPS settings");
    ImGui::TextWrapped("Save your changes, then restart the game to use them.");
    ImGui::Spacing();ImGui::SetNextItemWidth(160);ImGui::InputText("FPS target (0 = match monitor)",fps,sizeof(fps));
    ImGui::SetNextItemWidth(160);ImGui::InputText("FPS while Alt-Tabbed",background,sizeof(background));
    ImGui::Spacing();ImGui::Checkbox("High FPS in game",&game);ImGui::Checkbox("High FPS in menus",&menu);
    ImGui::Checkbox("Smooth movement",&smoothing);ImGui::Checkbox("Smooth weather",&weather);ImGui::Checkbox("Correct animation speed",&animations);
    ImGui::Spacing();
    if(ImGui::Button("Save FPS settings")){
        if(!valid_fps(fps)||!valid_fps(background))message="Enter a valid FPS number. Use 0 for automatic.";
        else try{
            auto yes=[](bool b){return b?"true":"false";};
            save_values({{"fps",fps},{"bg-fps",background},{"game-fps",yes(game)},{"menu-fps",yes(menu)},
                {"motion-smoothing",yes(smoothing)},{"weather-smoothing",yes(weather)},{"anim-rate-fixes",yes(animations)}});
            message="Saved. Restart the game to apply these settings.";
        }catch(const std::exception& e){message=e.what();}
    }
    if(!message.empty()){ImGui::Spacing();ImGui::TextWrapped("%s",message.c_str());}
}
}
