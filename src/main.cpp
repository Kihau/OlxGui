#include <stdio.h>

#include <GLFW/glfw3.h>

#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>

// NOTE: Should be removed, but is used for InputText activation.
#include <imgui/imgui_internal.h>

#include <string>
#include <vector>
#include <mutex>
#include <thread>

#if defined (_WIN32)
    #define process_open  _popen
    #define process_close _pclose
#else
    #define process_open  popen
    #define process_close pclose
#endif

// TODO: Stop previous query when a new one is created.

static bool demo = false;
static bool style = false;

struct GuiTab {
    bool is_enabled;

    int unique_id;
    std::string tab_name;
    std::string query_buffer;
    std::string regex_buffer;

    std::string min_price;
    std::string max_price;

    bool enable_condition;
    int selected_condition;

    bool enable_category;
    int selected_category;

    bool ignore_case;

    std::string include_buffer;
    std::string exclude_buffer;

    std::vector<std::string> excludes;
    std::vector<std::string> includes;

    std::mutex list_lock;
    std::vector<std::string> prices;
    std::vector<std::string> titles;
    std::vector<std::string> urls;
};

struct GuiData {
    std::vector<GuiTab*> tabs;
};

const char* condition_list[]       = { "Nowe", "Używane", "Uszkodzone" };
const char* condition_list_query[] = { "new", "used", "damaged" };
const char* category_list[]        = { 
    "Elektronika", "Motoryzacja", "Moda", "Praca", "Muzyka i Edukacja", "Dla dzieci", 
    "Zwierzęta", "Dom i Ogród", "Antyki i Kolekcje", "Sport i Hobby", "Rolnictwo", "Nieruchomości",
    "Dla Firm", "Zdrowie i Uroda", "Usługi", "Wypożyczalnia", "Oddam za darmo", "Noclegi"
};
const char* category_list_query[]  = { 
    "elektronika", "motoryzacja", "moda", "praca", "muzyka-edukacja", "dla-dzieci", "zwierzeta",
    "dom-ogrod", "antyki-i-kolekcje", "sport-hobby", "rolnictwo", "nieruchomosci", "dla-firm",
    "zdrowie-i-uroda", "uslugi", "wypozyczalnia", "oddam-za-darmo", "noclegi" 
};

static void glfw_error_callback(int error, const char *description) {
    printf("GLFW Error %d: %s\n", error, description);
}

static bool is_whitespace(const std::string &my_string) {
    if (my_string.size() == 0) {
        return true;
    }

    for (char c : my_string) {
        if (!isspace(c)) {
            return false;
        }
    }
    
    return true;
}


static bool is_number(const std::string &my_string) {
    if (my_string.size() == 0) {
        return false;
    }

    for (char c : my_string) {
        if (!isdigit(c)) {
            return false;
        }
    }
    
    return true;
}

static void execute_query(GuiTab *data) {
    // NOTE: Check whether previous query thread is running.
    data->list_lock.lock();
    data->prices.clear();
    data->titles.clear();
    data->urls.clear();
    data->list_lock.unlock();

    if (is_whitespace(data->query_buffer)) {
        return;
    }

    std::string query = "olx-find --delay 100";

    if (data->ignore_case) {
        query += " --ignore-case";
    }

    if (!is_whitespace(data->min_price)) {
        query += " --min ";
        query += data->min_price;
    }

    if (!is_whitespace(data->max_price)) {
        query += " --max ";
        query += data->max_price;
    }

    if (!is_whitespace(data->regex_buffer)) {
        query += " --regex ";
        query += "\"";
        query += data->regex_buffer;
        query += "\"";
    }

    if (data->enable_category) {
        query += " --category ";
        query += category_list_query[data->selected_category];
    }

    if (data->enable_condition) {
        query += " --condition ";
        query += condition_list_query[data->selected_condition];
    }

    for (const auto &exclude : data->excludes) {
        query += " --exclude \"";
        query += exclude;
        query += "\"";
    }

    for (const auto &include : data->includes) {
        query += " --include \"";
        query += include;
        query += "\"";
    }

    query += " \"";
    query += data->query_buffer;
    query += "\"";

    printf("Executing query: %s\n", query.c_str());

    // NOTE: Blocking version of the execute_query
    // auto command = process_open(query.c_str(), "r");
    // if (command == NULL) {
    //     printf("Failed to execute query\n");
    //     return;
    // }
    //
    // std::string output;
    // char buffer[2048];
    // while (fgets(buffer, sizeof(buffer), command)) {
    //     output += buffer;
    // }
    //
    // process_close(command);
    // #define separator "------------------------------------------------------------------------------------------"
    // auto end_of_separator = output.find(separator);
    // if (end_of_separator == -1) {
    //     return;
    // }
    // end_of_separator += sizeof(separator);
    // int line_start = end_of_separator;
    //
    // while (true) {
    //     auto cost_start = line_start;
    //     auto title_start = output.find(" ", cost_start) + 1;
    //     auto url_start = output.find("https://", cost_start);
    //     if (url_start == -1) {
    //         break;
    //     }
    //
    //     auto url_end = output.find("\n", url_start);
    //     if (url_end == -1) {
    //         break;
    //     }
    //
    //     line_start = url_end + 1;
    //
    //     std::string cost = output.substr(cost_start,  title_start - cost_start - 1);
    //     if (cost.size() == 1) {
    //         cost += "PLN";
    //     }
    //
    //     std::string title = output.substr(title_start, url_start - title_start - 1);
    //     for (auto &c : title) {
    //         if (c == '\n') c = ' ';
    //     }
    //     std::string url = output.substr(url_start,   url_end - url_start);
    //
    //     data->list_lock.lock();
    //     data->prices.push_back(cost);
    //     data->titles.push_back(title);
    //     data->urls.push_back(url);
    //     data->list_lock.unlock();
    // }

    std::thread query_thread([](GuiTab *data, std::string query) {
        auto command = process_open(query.c_str(), "r");
        if (command == NULL) {
            printf("Failed to execute query\n");
            return;
        }

        std::string output;
        char buffer[256];

        int separator_end;
        while (fgets(buffer, sizeof(buffer), command)) {
            output += buffer;

            #define separator "------------------------------------------------------------------------------------------"
            separator_end = output.find(separator);
            if (separator_end != -1) {
                separator_end += sizeof(separator);
                break;
            }
        }

        if (separator_end == -1) {
            printf("ERROR: Query execution failed\n");
            process_close(command);
            return;
        }

        int cursor = separator_end;

        while (fgets(buffer, sizeof(buffer), command)) {
            output += buffer;

            while (true) {
                auto cost_start = cursor;
                auto title_start = output.find(" ", cost_start) + 1;
                auto url_start = output.find("https://", cost_start);
                if (url_start == -1) {
                    break;
                }

                auto url_end = output.find("\n", url_start);
                if (url_end == -1) {
                    break;
                }

                cursor = url_end + 1;

                std::string cost = output.substr(cost_start,  title_start - cost_start - 1);
                if (cost.size() == 1) {
                    cost += "PLN";
                }

                std::string title = output.substr(title_start, url_start - title_start - 1);
                for (auto &c : title) {
                    if (c == '\n') c = ' ';
                }
                std::string url = output.substr(url_start,   url_end - url_start);

                data->list_lock.lock();
                data->prices.push_back(cost);
                data->titles.push_back(title);
                data->urls.push_back(url);
                data->list_lock.unlock();
            }
        }

        process_close(command);
    }, data, query);

    query_thread.detach();
}

static void filtering_layout(GuiTab *data) {
    ImGui::Text("Olx query filtering");
    ImGui::Separator();

    ImGui::Dummy(ImVec2(0.0f, 1.0f));
    ImGui::Text("Regex filter:");
    ImGui::SameLine();
    ImGui::PushItemWidth(ImGui::GetColumnWidth());
    ImGui::InputText("##regex_filtering_input", &data->regex_buffer);
    ImGui::Dummy(ImVec2(0.0f, 1.0f));

    ImGui::BeginGroup(); {
        auto width = (ImGui::GetColumnWidth() - 22.0) / 4.0;
        const float height = 75;

        if (ImGui::BeginChild("min_price_pane", ImVec2(width, height), ImGuiChildFlags_Border)) {
            ImGui::SeparatorText("Minimum price");
            ImGui::PushItemWidth(ImGui::GetColumnWidth());
            ImGui::InputText("##min_price", &data->min_price, ImGuiInputTextFlags_CharsDecimal);
        }
        ImGui::EndChild();

        ImGui::SameLine();

        if (ImGui::BeginChild("max_price_pane", ImVec2(width, height), ImGuiChildFlags_Border)) {
            ImGui::SeparatorText("Maximum price");
            ImGui::PushItemWidth(ImGui::GetColumnWidth());
            ImGui::InputText("##max_price", &data->max_price, ImGuiInputTextFlags_CharsDecimal);
        }
        ImGui::EndChild();

        ImGui::SameLine();

        if (ImGui::BeginChild("condition_pane", ImVec2(width, height), ImGuiChildFlags_Border)) {
            ImGui::SeparatorText("Item condition");

            ImGui::Checkbox("##condition_checkbox", &data->enable_condition);
            ImGui::SameLine();

            ImGui::BeginDisabled(!data->enable_condition);
            ImGui::PushItemWidth(ImGui::GetColumnWidth());
            ImGui::Combo("##item_condition_combo", &data->selected_condition, condition_list, IM_ARRAYSIZE(condition_list));
            ImGui::EndDisabled();
        }
        ImGui::EndChild();

        ImGui::SameLine();

        if (ImGui::BeginChild("category_pane", ImVec2(width, height), ImGuiChildFlags_Border)) {
            ImGui::SeparatorText("Item category");

            ImGui::Checkbox("##category_checkbox", &data->enable_category);
            ImGui::SameLine();

            ImGui::BeginDisabled(!data->enable_category);
            ImGui::PushItemWidth(ImGui::GetColumnWidth());
            ImGui::Combo("##item_category_combo", &data->selected_category, category_list, IM_ARRAYSIZE(category_list));
            ImGui::EndDisabled();
        }
        ImGui::EndChild();


        if (ImGui::BeginChild("excludes_pane", ImVec2(ImGui::GetColumnWidth() / 2.0, 0), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX)) {
            ImGui::Text("Exclude keywords");
            ImGui::Separator();

            if (ImGui::Button("Add##exclude_add")) {
                if (!is_whitespace(data->exclude_buffer)) {
                    data->excludes.push_back(data->exclude_buffer);
                    data->exclude_buffer.clear();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset##exclude_reset")) { 
                data->excludes.clear();
            }
            ImGui::SameLine();
            ImGui::PushItemWidth(ImGui::GetColumnWidth());
            if (ImGui::InputText("##exclude_filter_input", &data->exclude_buffer, ImGuiInputTextFlags_EnterReturnsTrue)) {
                auto id = ImGui::GetItemID();
                ImGui::ActivateItemByID(id);

                if (!is_whitespace(data->exclude_buffer)) {
                    data->excludes.push_back(data->exclude_buffer);
                    data->exclude_buffer.clear();
                }
            }

            if (ImGui::CollapsingHeader("Added excludes")) {
                for (size_t i = 0; i < data->excludes.size(); i++) {
                    auto &exclude = data->excludes[i];

                    char id_buffer[64];
                    sprintf(id_buffer, "exclude_%li", i);
                    ImGui::PushID(id_buffer);

                    if (ImGui::Selectable(exclude.c_str())) {
                        data->excludes.erase(data->excludes.begin() + i);
                        ImGui::PopID();
                        break;
                    }

                    ImGui::PopID();
                }
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();

        if (ImGui::BeginChild("includes_pane", ImVec2(0, 0), ImGuiChildFlags_Border)) {
            ImGui::Text("Include keywords");
            ImGui::Separator();

            if (ImGui::Button("Add##include_add")) {
                if (!is_whitespace(data->include_buffer)) {
                    data->includes.push_back(data->include_buffer);
                    data->include_buffer.clear();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset##include_reset")) { 
                data->includes.clear();
            }

            ImGui::SameLine();
            ImGui::PushItemWidth(ImGui::GetColumnWidth());
            if (ImGui::InputText("##include_filter_input", &data->include_buffer, ImGuiInputTextFlags_EnterReturnsTrue)) {
                auto id = ImGui::GetItemID();
                ImGui::ActivateItemByID(id);

                if (!is_whitespace(data->include_buffer)) {
                    data->includes.push_back(data->include_buffer);
                    data->include_buffer.clear();
                }
            }

            if (ImGui::CollapsingHeader("Added includes")) {
                for (size_t i = 0; i < data->includes.size(); i++) {
                    auto &include = data->includes[i];

                    char id_buffer[64];
                    sprintf(id_buffer, "include_%li", i);
                    ImGui::PushID(id_buffer);

                    if (ImGui::Selectable(include.c_str())) {
                        data->includes.erase(data->includes.begin() + i);
                        ImGui::PopID();
                        break;
                    }

                    ImGui::PopID();
                }
            }
        }
        ImGui::EndChild();
    } 
    ImGui::EndGroup();
}

static void single_tab_layout(GuiTab *data) {
    ImGui::BeginGroup(); {
        if (ImGui::BeginChild("search_pane", ImVec2(0, 115), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeY)) {
            ImGui::Text("Olx input query");
            ImGui::Separator();

            ImGui::Dummy(ImVec2(0.0f, 1.0f));
            ImGui::Text("Your query: ");
            ImGui::SameLine();
            ImGui::PushItemWidth(ImGui::GetColumnWidth());
            if (ImGui::InputText("##olx_query_input", &data->query_buffer, ImGuiInputTextFlags_EnterReturnsTrue)) {
                execute_query(data);
            }

            ImGui::Checkbox("Ignore case", &data->ignore_case);
            ImGui::SameLine();

            if (ImGui::Button("Search")) {
                data->tab_name = data->query_buffer;
                if (data->unique_id == 0) data->unique_id = rand();
                data->tab_name += "##named_tab_unique_id";
                data->tab_name += std::to_string(data->unique_id);

                execute_query(data);
            }

            ImGui::SameLine();

            if (ImGui::Button("Clear")) {
                data->query_buffer.clear();
            }

            // ImGui::SameLine();
            // if (ImGui::Button("Demo")) {
            //     demo = true;
            // }

            ImGui::SameLine();
            if (ImGui::Button("Style")) {
                style ^= 1;
            }
        }
        ImGui::EndChild();

        if (ImGui::BeginChild("filtering_pane", ImVec2(0, 300), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeY)) {
            filtering_layout(data);
        }
        ImGui::EndChild();

        if (ImGui::BeginChild("output_pane", ImVec2(0, 0), ImGuiChildFlags_Border)) {
            if (ImGui::Button("Clear")) {
                data->list_lock.lock();
                data->prices.clear();
                data->titles.clear();
                data->urls.clear();
                data->list_lock.unlock();
            }
            ImGui::SameLine();
            ImGui::Text("Item list");
            ImGui::Separator();

            static ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg     |
                                           ImGuiTableFlags_Borders        | ImGuiTableFlags_Resizable | 
                                           ImGuiTableFlags_Reorderable    | ImGuiTableFlags_Hideable  | 
                                           ImGuiTableFlags_ScrollY;

            if (ImGui::BeginTable("query_results", 2, flags)) {
                ImGui::TableSetupColumn("Prices", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Title",  ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableHeadersRow();

                data->list_lock.lock();
                for (int i = 0; i < data->prices.size(); i++) {
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%s", data->prices[i].c_str());

                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextLinkOpenURL(data->titles[i].c_str(), data->urls[i].c_str());
                }
                data->list_lock.unlock();
                ImGui::EndTable();
            }
        }
        ImGui::EndChild();
    }
    ImGui::EndGroup();

}

GuiTab* create_new_tab() {
    std::string new_tab = "New Tab ";
    new_tab += "##new_tab_dummy_id";
    new_tab += std::to_string(rand());

    GuiTab *tab = new GuiTab;
    tab->is_enabled = true;

    tab->unique_id = 0;
    tab->tab_name  = new_tab;
    tab->enable_condition   = false;
    tab->selected_condition = 0;

    tab->enable_category    = false;
    tab->selected_category  = 0;

    tab->ignore_case = true;

    return tab;
}

static void main_layout(GuiData *data) {
    // if (demo) {
    //     ImGui::ShowDemoWindow(&demo);
    // }

    if (style) {
        ImGui::ShowStyleEditor();
    }

    ImGui::SetNextWindowBgAlpha(1.0f);
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

    bool always_on = true;
    auto root_window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("root_window", &always_on, root_window_flags);
    ImGui::PopStyleVar(1);


    const ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs;

    if (ImGui::BeginTabBar("##main_layout_tab_bar", tab_bar_flags)) {
        for (size_t i = 0; i < data->tabs.size(); i++) {
            // NOTE: Disabling the tab and not removing it leaks memory.
            if (ImGui::BeginTabItem(data->tabs[i]->tab_name.c_str(), &data->tabs[i]->is_enabled)) {
                single_tab_layout(data->tabs[i]);
                ImGui::EndTabItem();
            }

            // NOTE: A thread might still be running and corrupt other data. Use this version once the
            //       thread stopping logic is nice and polished.
            // bool is_open = true;
            // if (ImGui::BeginTabItem(data->tabs[i]->tab_name.c_str(), &is_open)) {
            //     single_tab_layout(data->tabs[i]);
            //     ImGui::EndTabItem();
            // }
            //
            // if (!is_open) {
            //     data->tabs.erase(data->tabs.begin() + i);
            //     break;
            // }
        }

        if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip)) {
            // NOTE: I could resuse closed tabs instead of always creating new ones.
            GuiTab *new_tab = create_new_tab();
            data->tabs.push_back(new_tab);
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

void write_line(FILE *file, const std::string &line) {
    // NOTE: It might be important to replace all new lines within strings with spaces.
    fwrite(line.c_str(), 1, line.size(), file);
    fwrite("\n", 1, 1, file);
}

static void save_tabs(GuiData *data) {
    auto file = fopen("olx_data.txt", "w");
    if (file == nullptr) {
        fprintf(stderr, "ERROR: Failed to save data\n");
        return;
    }

    for (const auto tab : data->tabs) {
        if (!tab->is_enabled) {
            continue;
        }

        write_line(file, std::to_string(tab->unique_id));
        write_line(file, tab->tab_name);
        write_line(file, tab->query_buffer);
        write_line(file, tab->regex_buffer);
        write_line(file, tab->min_price);
        write_line(file, tab->max_price);
        write_line(file, std::to_string(tab->enable_condition));
        write_line(file, std::to_string(tab->selected_condition));
        write_line(file, std::to_string(tab->enable_category));
        write_line(file, std::to_string(tab->selected_category));
        write_line(file, std::to_string(tab->ignore_case));

        write_line(file, " ");
        for (const auto &exclude : tab->excludes) {
            write_line(file, exclude);
        }

        write_line(file, " ");
        for (const auto &include : tab->includes) {
            write_line(file, include);
        }

        write_line(file, " ");
        write_line(file, " ");
    }

    fclose(file);
}

#include <fstream>

static void load_tabs(GuiData *data) {
    std::ifstream file("olx_data.txt");
    if (!file.is_open()) {
        return;
    }

    std::string temp;
    while (true) {
        GuiTab *tab = create_new_tab();

        if (!std::getline(file, temp)) break;
        if (is_number(temp)) tab->unique_id = std::stoi(temp);

        if (!std::getline(file, temp)) break;
        if (!is_whitespace(temp)) tab->tab_name = temp;

        if (!std::getline(file, temp)) break;
        if (!is_whitespace(temp)) tab->query_buffer = temp;

        if (!std::getline(file, temp)) break;
        if (!is_whitespace(temp)) tab->regex_buffer = temp;

        if (!std::getline(file, temp)) break;
        if (!is_whitespace(temp)) tab->min_price = temp;

        if (!std::getline(file, temp)) break;
        if (!is_whitespace(temp)) tab->max_price = temp;

        if (!std::getline(file, temp)) break;
        if (is_number(temp)) tab->enable_condition = std::stoi(temp);
        if (!std::getline(file, temp)) break;
        if (is_number(temp)) tab->selected_condition = std::stoi(temp);

        if (!std::getline(file, temp)) break;
        if (is_number(temp)) tab->enable_category = std::stoi(temp);
        if (!std::getline(file, temp)) break;
        if (is_number(temp)) tab->selected_category = std::stoi(temp);

        if (!std::getline(file, temp)) break;
        if (is_number(temp)) tab->ignore_case = std::stoi(temp);

        if (!std::getline(file, temp)) break;

        if (!std::getline(file, temp)) break;
        while (!is_whitespace(temp)) {
            tab->excludes.push_back(temp);
            if (!std::getline(file, temp)) goto exit_parsing;
        }

        if (!std::getline(file, temp)) break;
        while (!is_whitespace(temp)) {
            tab->includes.push_back(temp);
            if (!std::getline(file, temp)) goto exit_parsing;
        }

        std::getline(file, temp);

        data->tabs.push_back(tab);
    }

exit_parsing:

    file.close();
}


static void apply_style_other() {
    auto &style = ImGui::GetStyle();

    // Main
    style.WindowPadding = ImVec2(12.0f, 10.0f);
    style.FramePadding  = ImVec2(10.0f, 5.0f);
    style.ItemSpacing   = ImVec2(10.0f, 5.0f);

    // Rounding
    style.WindowRounding     = 10.0f;
    style.ChildRounding      = 6.0f;
    style.FrameRounding      = 4.0f;
    style.PopupRounding      = 5.0f;
    style.ScrollbarRounding  = 10.0f;
    style.GrabRounding       = 3.0f;
    style.TabRounding        = 4.0f;

    // Tables
    style.CellPadding = ImVec2(10.0f, 5.0f);

    // Widgets
    style.WindowTitleAlign    = ImVec2(0.5f,  0.5f);
    style.SelectableTextAlign = ImVec2(0.03f, 0.0f);
    style.SeparatorTextAlign  = ImVec2(0.5f,  0.5f);
    
}

static void apply_style_colors() {
    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_Text]                       = ImVec4(0.78f, 0.82f, 0.96f, 1.00f);
    colors[ImGuiCol_TextDisabled]               = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]                   = ImVec4(0.19f, 0.20f, 0.27f, 1.00f);
    colors[ImGuiCol_ChildBg]                    = ImVec4(0.16f, 0.17f, 0.24f, 1.00f);
    colors[ImGuiCol_PopupBg]                    = ImVec4(0.16f, 0.17f, 0.24f, 1.00f);
    colors[ImGuiCol_Border]                     = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    colors[ImGuiCol_BorderShadow]               = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                    = ImVec4(0.14f, 0.15f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]             = ImVec4(0.31f, 0.37f, 0.51f, 1.00f);
    colors[ImGuiCol_FrameBgActive]              = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    colors[ImGuiCol_TitleBg]                    = ImVec4(0.08f, 0.08f, 0.11f, 1.00f);
    colors[ImGuiCol_TitleBgActive]              = ImVec4(0.11f, 0.12f, 0.16f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]           = ImVec4(0.04f, 0.05f, 0.06f, 0.49f);
    colors[ImGuiCol_MenuBarBg]                  = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]                = ImVec4(0.13f, 0.14f, 0.18f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab]              = ImVec4(0.25f, 0.27f, 0.35f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]       = ImVec4(0.32f, 0.34f, 0.43f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]        = ImVec4(0.38f, 0.41f, 0.50f, 1.00f);
    colors[ImGuiCol_CheckMark]                  = ImVec4(0.78f, 0.82f, 0.96f, 1.00f);
    colors[ImGuiCol_SliderGrab]                 = ImVec4(0.55f, 0.67f, 0.93f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]           = ImVec4(0.73f, 0.73f, 0.95f, 1.00f);
    colors[ImGuiCol_Button]                     = ImVec4(0.25f, 0.27f, 0.35f, 1.00f);
    colors[ImGuiCol_ButtonHovered]              = ImVec4(0.47f, 0.56f, 0.78f, 1.00f);
    colors[ImGuiCol_ButtonActive]               = ImVec4(0.61f, 0.64f, 0.76f, 1.00f);
    colors[ImGuiCol_Header]                     = ImVec4(0.32f, 0.34f, 0.43f, 1.00f);
    colors[ImGuiCol_HeaderHovered]              = ImVec4(0.38f, 0.41f, 0.50f, 1.00f);
    colors[ImGuiCol_HeaderActive]               = ImVec4(0.45f, 0.47f, 0.58f, 1.00f);
    colors[ImGuiCol_Separator]                  = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    colors[ImGuiCol_SeparatorHovered]           = ImVec4(0.45f, 0.47f, 0.58f, 1.00f);
    colors[ImGuiCol_SeparatorActive]            = ImVec4(0.51f, 0.55f, 0.65f, 1.00f);
    colors[ImGuiCol_ResizeGrip]                 = ImVec4(0.40f, 0.42f, 0.50f, 1.00f);
    colors[ImGuiCol_ResizeGripHovered]          = ImVec4(0.61f, 0.64f, 0.76f, 1.00f);
    colors[ImGuiCol_ResizeGripActive]           = ImVec4(0.69f, 0.73f, 0.86f, 1.00f);
    colors[ImGuiCol_TabHovered]                 = ImVec4(0.58f, 0.61f, 0.73f, 1.00f);
    colors[ImGuiCol_Tab]                        = ImVec4(0.38f, 0.41f, 0.50f, 1.00f);
    colors[ImGuiCol_TabSelected]                = ImVec4(0.51f, 0.55f, 0.65f, 1.00f);
    colors[ImGuiCol_TabSelectedOverline]        = ImVec4(0.65f, 0.68f, 0.81f, 1.00f);
    colors[ImGuiCol_TabDimmed]                  = ImVec4(0.07f, 0.10f, 0.15f, 0.97f);
    colors[ImGuiCol_TabDimmedSelected]          = ImVec4(0.14f, 0.26f, 0.42f, 1.00f);
    colors[ImGuiCol_TabDimmedSelectedOverline]  = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_PlotLines]                  = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]           = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram]              = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]       = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]              = ImVec4(0.14f, 0.15f, 0.20f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]          = ImVec4(0.32f, 0.34f, 0.43f, 1.00f);
    colors[ImGuiCol_TableBorderLight]           = ImVec4(0.25f, 0.27f, 0.35f, 1.00f);
    colors[ImGuiCol_TableRowBg]                 = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]              = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    colors[ImGuiCol_TextLink]                   = ImVec4(0.55f, 0.67f, 0.93f, 1.00f);
    colors[ImGuiCol_TextSelectedBg]             = ImVec4(0.46f, 0.46f, 0.60f, 1.00f);
    colors[ImGuiCol_DragDropTarget]             = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight]               = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight]      = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]          = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]           = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
}

int main() {
    srand(time(NULL));

    GuiData data = { };

    load_tabs(&data);
    if (data.tabs.size() == 0) {
        GuiTab *tab = create_new_tab();
        data.tabs.push_back(tab);
    }

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        return 1;
    }

    const char *glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    // glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only

    // Create window with graphics context
    GLFWwindow *window = glfwCreateWindow(1100, 800, "Olx GUI", nullptr, nullptr);
    if (window == nullptr) {
        // return 1;
    }

    glfwMakeContextCurrent(window);

    // Enable vsync
    glfwSwapInterval(1);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImVector<ImWchar> ranges;
    ImFontGlyphRangesBuilder builder;
    builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
    builder.AddText("ąćęłńóśżźĄĆĘŁŃÓŚŻŹ");
    builder.BuildRanges(&ranges);
    io.Fonts->AddFontFromFileTTF("Roboto-Regular.ttf", 16.0f, nullptr, ranges.Data);
    io.Fonts->Build();

    ImGui::StyleColorsDark();
    apply_style_colors();
    apply_style_other();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    ImVec4 clear_color = ImVec4(1.0, 1.0, 1.0, 1.0);

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Begin frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // single_tab_layout(&data);
        main_layout(&data);

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(
            clear_color.x * clear_color.w, clear_color.y * clear_color.w,
            clear_color.z * clear_color.w, clear_color.w
        );
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    save_tabs(&data);

    return 0;
}
