cmake_minimum_required(VERSION 3.25)

project(guideXOSAppModelWindows VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(guidexos_appmodel STATIC
    src/appmodel/application.cpp
    src/appmodel/clipboard.cpp
    src/appmodel/controls.cpp
    src/appmodel/dialogs.cpp
    src/appmodel/file.cpp
    src/appmodel/file_drop.cpp
    src/appmodel/layout.cpp
    src/appmodel/menu.cpp
    src/appmodel/runtime.cpp
    src/appmodel/status_bar.cpp
    src/appmodel/timer.cpp
    src/appmodel/text_index.cpp
    src/appmodel/text_validation.cpp
    src/appmodel/window.cpp
    src/platform/windows/windows_backend.cpp
    src/platform/windows/clipboard_backend.cpp
    src/platform/windows/file_backend.cpp
)

target_include_directories(guidexos_appmodel
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_definitions(guidexos_appmodel PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(guidexos_appmodel PRIVATE /W4 /WX /permissive-)
else()
    target_compile_options(guidexos_appmodel PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

target_link_libraries(guidexos_appmodel PRIVATE user32 ole32 shell32 comctl32)

add_executable(HelloApp WIN32
    samples/HelloApp/main.cpp
)
target_link_libraries(HelloApp PRIVATE guidexos_appmodel)

add_executable(MultiWindowApp WIN32
    samples/MultiWindowApp/main.cpp
)
target_link_libraries(MultiWindowApp PRIVATE guidexos_appmodel)

add_executable(TextInputApp WIN32
    samples/TextInputApp/main.cpp
)
target_link_libraries(TextInputApp PRIVATE guidexos_appmodel)

add_executable(ListBoxApp WIN32
    samples/ListBoxApp/main.cpp
)
target_link_libraries(ListBoxApp PRIVATE guidexos_appmodel)

add_executable(ChoiceControlsApp WIN32
    samples/ChoiceControlsApp/main.cpp
)
target_link_libraries(ChoiceControlsApp PRIVATE guidexos_appmodel)

add_executable(ComboBoxApp WIN32
    samples/ComboBoxApp/main.cpp
)
target_link_libraries(ComboBoxApp PRIVATE guidexos_appmodel)

add_library(profile_manager_model STATIC
    samples/ProfileManagerApp/profile_manager_model.cpp
)
target_include_directories(profile_manager_model
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/samples/ProfileManagerApp"
)

add_executable(ProfileManagerApp WIN32
    samples/ProfileManagerApp/main.cpp
    tests/file_drop_in_process_helper.cpp
    tests/status_bar_in_process_helper.cpp
)
target_link_libraries(ProfileManagerApp PRIVATE
    guidexos_appmodel
    profile_manager_model
)
target_compile_definitions(ProfileManagerApp PRIVATE
    GUIDEXOS_FILE_DROP_TEST_HELPER
    GUIDEXOS_STATUS_TEST_HELPER
)

add_executable(MenuApp WIN32
    samples/MenuApp/main.cpp
)
target_link_libraries(MenuApp PRIVATE guidexos_appmodel)

add_executable(DialogApp WIN32
    samples/DialogApp/main.cpp
)
target_link_libraries(DialogApp PRIVATE guidexos_appmodel)

add_executable(ClipboardApp WIN32
    samples/ClipboardApp/main.cpp
)
target_link_libraries(ClipboardApp PRIVATE guidexos_appmodel)

add_executable(TextEditingApp WIN32
    samples/TextEditingApp/main.cpp
)
target_link_libraries(TextEditingApp PRIVATE guidexos_appmodel)

add_executable(MultilineTextApp WIN32
    samples/MultilineTextApp/main.cpp
)
target_link_libraries(MultilineTextApp PRIVATE guidexos_appmodel)

add_executable(FocusCommandApp WIN32
    samples/FocusCommandApp/main.cpp
)
target_link_libraries(FocusCommandApp PRIVATE guidexos_appmodel)

add_executable(FileDropApp WIN32
    samples/FileDropApp/main.cpp
    tests/file_drop_in_process_helper.cpp
)
target_link_libraries(FileDropApp PRIVATE guidexos_appmodel)
target_compile_definitions(FileDropApp PRIVATE GUIDEXOS_FILE_DROP_TEST_HELPER)

add_executable(PolishApp WIN32
    samples/PolishApp/main.cpp
    tests/status_bar_in_process_helper.cpp
)
target_link_libraries(PolishApp PRIVATE guidexos_appmodel)
target_compile_definitions(PolishApp PRIVATE GUIDEXOS_STATUS_TEST_HELPER)

add_executable(TimerApp WIN32
    samples/TimerApp/main.cpp
)
target_link_libraries(TimerApp PRIVATE guidexos_appmodel)

add_executable(ProgressBarApp WIN32
    samples/ProgressBarApp/main.cpp
)
target_link_libraries(ProgressBarApp PRIVATE guidexos_appmodel)

if(MSVC)
    # Keep the sample's source-level entry point platform-neutral while
    # selecting the Windows GUI subsystem (and therefore no console window).
    target_link_options(HelloApp PRIVATE /ENTRY:mainCRTStartup)
    target_link_options(MultiWindowApp PRIVATE /ENTRY:mainCRTStartup)
    target_link_options(TextInputApp PRIVATE /ENTRY:mainCRTStartup)
    target_link_options(ListBoxApp PRIVATE /ENTRY:mainCRTStartup)
    target_link_options(ChoiceControlsApp PRIVATE /ENTRY:mainCRTStartup)
    target_link_options(ComboBoxApp PRIVATE /ENTRY:mainCRTStartup)
    target_link_options(ProfileManagerApp PRIVATE /ENTRY:mainCRTStartup)
    target_link_options(MenuApp PRIVATE /ENTRY:mainCRTStartup)
    target_link_options(DialogApp PRIVATE /ENTRY:mainCRTStartup)
    target_link_options(ClipboardApp PRIVATE /ENTRY:mainCRTStartup)
    target_link_options(TextEditingApp PRIVATE /ENTRY:mainCRTStartup)
    target_link_options(MultilineTextApp PRIVATE /ENTRY:mainCRTStartup)
    target_link_options(FocusCommandApp PRIVATE /ENTRY:mainCRTStartup)
    target_link_options(FileDropApp PRIVATE /ENTRY:mainCRTStartup)
    target_link_options(PolishApp PRIVATE /ENTRY:mainCRTStartup)
    target_link_options(TimerApp PRIVATE /ENTRY:mainCRTStartup)
    target_link_options(ProgressBarApp PRIVATE /ENTRY:mainCRTStartup)
endif()

if(MSVC)
    target_compile_options(HelloApp PRIVATE /W4 /WX /permissive-)
    target_compile_options(MultiWindowApp PRIVATE /W4 /WX /permissive-)
    target_compile_options(TextInputApp PRIVATE /W4 /WX /permissive-)
    target_compile_options(ListBoxApp PRIVATE /W4 /WX /permissive-)
    target_compile_options(ChoiceControlsApp PRIVATE /W4 /WX /permissive-)
    target_compile_options(ComboBoxApp PRIVATE /W4 /WX /permissive-)
    target_compile_options(ProfileManagerApp PRIVATE /W4 /WX /permissive-)
    target_compile_options(MenuApp PRIVATE /W4 /WX /permissive-)
    target_compile_options(DialogApp PRIVATE /W4 /WX /permissive-)
    target_compile_options(ClipboardApp PRIVATE /W4 /WX /permissive-)
    target_compile_options(TextEditingApp PRIVATE /W4 /WX /permissive-)
    target_compile_options(MultilineTextApp PRIVATE /W4 /WX /permissive-)
    target_compile_options(FocusCommandApp PRIVATE /W4 /WX /permissive-)
    target_compile_options(FileDropApp PRIVATE /W4 /WX /permissive-)
    target_compile_options(PolishApp PRIVATE /W4 /WX /permissive-)
    target_compile_options(TimerApp PRIVATE /W4 /WX /permissive-)
    target_compile_options(ProgressBarApp PRIVATE /W4 /WX /permissive-)
    target_compile_options(profile_manager_model PRIVATE /W4 /WX /permissive-)
else()
    target_compile_options(HelloApp PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(MultiWindowApp PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(TextInputApp PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(ListBoxApp PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(ChoiceControlsApp PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(ComboBoxApp PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(ProfileManagerApp PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(MenuApp PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(DialogApp PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(ClipboardApp PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(TextEditingApp PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(MultilineTextApp PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(FocusCommandApp PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(FileDropApp PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(PolishApp PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(profile_manager_model PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(TimerApp PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(ProgressBarApp PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_executable(appmodel_model_test
    tests/model_behavior_test.cpp
)
target_link_libraries(appmodel_model_test PRIVATE guidexos_appmodel)

add_executable(appmodel_lifecycle_test
    tests/lifecycle_behavior_test.cpp
)
target_link_libraries(appmodel_lifecycle_test PRIVATE guidexos_appmodel)

add_executable(appmodel_window_closing_test
    tests/window_closing_behavior_test.cpp
)
target_link_libraries(appmodel_window_closing_test PRIVATE guidexos_appmodel)

add_executable(appmodel_listbox_model_test
    tests/listbox_model_behavior_test.cpp
)
target_link_libraries(appmodel_listbox_model_test PRIVATE guidexos_appmodel)

add_executable(appmodel_listbox_lifecycle_test
    tests/listbox_lifecycle_behavior_test.cpp
)
target_link_libraries(appmodel_listbox_lifecycle_test PRIVATE guidexos_appmodel)
target_link_libraries(appmodel_listbox_lifecycle_test PRIVATE user32)

add_executable(appmodel_choice_model_test
    tests/choice_model_behavior_test.cpp
)
target_link_libraries(appmodel_choice_model_test PRIVATE guidexos_appmodel)

add_executable(appmodel_choice_lifecycle_test
    tests/choice_lifecycle_behavior_test.cpp
)
target_link_libraries(appmodel_choice_lifecycle_test PRIVATE guidexos_appmodel)
target_link_libraries(appmodel_choice_lifecycle_test PRIVATE user32)

add_executable(appmodel_combobox_model_test
    tests/combobox_model_behavior_test.cpp
)
target_link_libraries(appmodel_combobox_model_test PRIVATE guidexos_appmodel)

add_executable(appmodel_combobox_lifecycle_test
    tests/combobox_lifecycle_behavior_test.cpp
)
target_link_libraries(appmodel_combobox_lifecycle_test PRIVATE guidexos_appmodel)
target_link_libraries(appmodel_combobox_lifecycle_test PRIVATE user32)

add_executable(profile_manager_model_test
    tests/profile_manager_model_test.cpp
)
target_link_libraries(profile_manager_model_test PRIVATE profile_manager_model)

add_executable(appmodel_listbox_textbox_sync_test
    tests/listbox_textbox_sync_test.cpp
)
target_link_libraries(appmodel_listbox_textbox_sync_test PRIVATE guidexos_appmodel)
target_link_libraries(appmodel_listbox_textbox_sync_test PRIVATE user32)

add_executable(appmodel_layout_test
    tests/layout_behavior_test.cpp
)
target_link_libraries(appmodel_layout_test PRIVATE guidexos_appmodel)

add_executable(appmodel_layout_measurement_test
    tests/layout_measurement_test.cpp
)
target_link_libraries(appmodel_layout_measurement_test PRIVATE guidexos_appmodel)
target_include_directories(appmodel_layout_measurement_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

add_executable(appmodel_menu_model_test
    tests/menu_model_behavior_test.cpp
)
target_link_libraries(appmodel_menu_model_test PRIVATE guidexos_appmodel)

add_executable(appmodel_dialog_model_test
    tests/dialog_model_behavior_test.cpp
)
target_link_libraries(appmodel_dialog_model_test PRIVATE guidexos_appmodel)

add_executable(appmodel_file_api_test
    tests/file_api_test.cpp
)
target_link_libraries(appmodel_file_api_test PRIVATE guidexos_appmodel)

add_executable(appmodel_clipboard_test
    tests/clipboard_model_behavior_test.cpp
)
target_link_libraries(appmodel_clipboard_test PRIVATE guidexos_appmodel user32)

add_executable(appmodel_text_box_test
    tests/text_box_model_behavior_test.cpp
)
target_link_libraries(appmodel_text_box_test PRIVATE guidexos_appmodel user32)

add_executable(appmodel_text_area_test
    tests/text_area_model_behavior_test.cpp
)
target_link_libraries(appmodel_text_area_test PRIVATE guidexos_appmodel user32)

add_executable(appmodel_focus_model_test
    tests/focus_model_behavior_test.cpp
)
target_link_libraries(appmodel_focus_model_test PRIVATE guidexos_appmodel)

add_executable(appmodel_file_drop_model_test
    tests/file_drop_model_test.cpp
)
target_link_libraries(appmodel_file_drop_model_test PRIVATE guidexos_appmodel)
target_include_directories(appmodel_file_drop_model_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_compile_definitions(appmodel_file_drop_model_test PRIVATE
    GUIDEXOS_FILE_DROP_MODEL_TEST_ACCESS
)

add_executable(appmodel_file_drop_native_test
    tests/file_drop_native_test.cpp
)
target_link_libraries(appmodel_file_drop_native_test PRIVATE guidexos_appmodel user32 shell32)

add_executable(appmodel_tooltip_statusbar_test
    tests/tooltip_statusbar_test.cpp
)
target_link_libraries(appmodel_tooltip_statusbar_test PRIVATE
    guidexos_appmodel user32 shell32 comctl32
)

add_executable(appmodel_timer_test
    tests/timer_model_behavior_test.cpp
)
target_link_libraries(appmodel_timer_test PRIVATE guidexos_appmodel)

add_executable(appmodel_progress_bar_model_test
    tests/progress_bar_model_behavior_test.cpp
)
target_link_libraries(appmodel_progress_bar_model_test PRIVATE guidexos_appmodel)

add_executable(appmodel_progress_bar_lifecycle_test
    tests/progress_bar_lifecycle_behavior_test.cpp
)
target_link_libraries(appmodel_progress_bar_lifecycle_test PRIVATE
    guidexos_appmodel user32 comctl32
)

if(MSVC)
    target_compile_options(appmodel_model_test PRIVATE /W4 /WX /permissive-)
    target_compile_options(appmodel_lifecycle_test PRIVATE /W4 /WX /permissive-)
    target_compile_options(appmodel_window_closing_test PRIVATE /W4 /WX /permissive-)
    target_compile_options(appmodel_listbox_model_test PRIVATE /W4 /WX /permissive-)
    target_compile_options(appmodel_listbox_lifecycle_test PRIVATE /W4 /WX /permissive-)
    target_compile_options(appmodel_choice_model_test PRIVATE /W4 /WX /permissive-)
    target_compile_options(appmodel_choice_lifecycle_test PRIVATE /W4 /WX /permissive-)
    target_compile_options(appmodel_combobox_model_test PRIVATE /W4 /WX /permissive-)
    target_compile_options(appmodel_combobox_lifecycle_test PRIVATE /W4 /WX /permissive-)
    target_compile_options(profile_manager_model_test PRIVATE /W4 /WX /permissive-)
    target_compile_options(appmodel_listbox_textbox_sync_test PRIVATE /W4 /WX /permissive-)
    target_compile_options(appmodel_layout_test PRIVATE /W4 /WX /permissive-)
    target_compile_options(appmodel_layout_measurement_test PRIVATE /W4 /WX /permissive-)
    target_compile_options(appmodel_menu_model_test PRIVATE /W4 /WX /permissive-)
    target_compile_options(appmodel_dialog_model_test PRIVATE /W4 /WX /permissive-)
    target_compile_options(appmodel_file_api_test PRIVATE /W4 /WX /permissive-)
    target_compile_options(appmodel_clipboard_test PRIVATE /W4 /WX /permissive-)
    target_compile_options(appmodel_text_box_test PRIVATE /W4 /WX /permissive-)
    target_compile_options(appmodel_text_area_test PRIVATE /W4 /WX /permissive-)
    target_compile_options(appmodel_focus_model_test PRIVATE /W4 /WX /permissive-)
    target_compile_options(appmodel_file_drop_model_test PRIVATE /W4 /WX /permissive-)
    target_compile_options(appmodel_file_drop_native_test PRIVATE /W4 /WX /permissive-)
    target_compile_options(appmodel_tooltip_statusbar_test PRIVATE /W4 /WX /permissive-)
    target_compile_options(appmodel_timer_test PRIVATE /W4 /WX /permissive-)
    target_compile_options(appmodel_progress_bar_model_test PRIVATE /W4 /WX /permissive-)
    target_compile_options(appmodel_progress_bar_lifecycle_test PRIVATE /W4 /WX /permissive-)
else()
    target_compile_options(appmodel_model_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(appmodel_lifecycle_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(appmodel_window_closing_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(appmodel_listbox_model_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(appmodel_listbox_lifecycle_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(appmodel_choice_model_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(appmodel_choice_lifecycle_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(appmodel_combobox_model_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(appmodel_combobox_lifecycle_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(profile_manager_model_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(appmodel_listbox_textbox_sync_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(appmodel_layout_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(appmodel_layout_measurement_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(appmodel_menu_model_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(appmodel_dialog_model_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(appmodel_file_api_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(appmodel_clipboard_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(appmodel_text_box_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(appmodel_text_area_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(appmodel_focus_model_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(appmodel_file_drop_model_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(appmodel_file_drop_native_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(appmodel_tooltip_statusbar_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(appmodel_timer_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(appmodel_progress_bar_model_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(appmodel_progress_bar_lifecycle_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

enable_testing()
add_test(NAME appmodel_model_behavior COMMAND appmodel_model_test)
add_test(NAME appmodel_lifecycle_behavior COMMAND appmodel_lifecycle_test)
add_test(NAME appmodel_window_closing_behavior COMMAND appmodel_window_closing_test)
add_test(NAME appmodel_listbox_model_behavior COMMAND appmodel_listbox_model_test)
add_test(NAME appmodel_listbox_lifecycle_behavior COMMAND appmodel_listbox_lifecycle_test)
add_test(NAME appmodel_choice_model_behavior COMMAND appmodel_choice_model_test)
add_test(NAME appmodel_choice_lifecycle_behavior COMMAND appmodel_choice_lifecycle_test)
add_test(NAME appmodel_combobox_model_behavior COMMAND appmodel_combobox_model_test)
add_test(NAME appmodel_combobox_lifecycle_behavior COMMAND appmodel_combobox_lifecycle_test)
add_test(NAME profile_manager_model_behavior COMMAND profile_manager_model_test)
add_test(NAME appmodel_listbox_textbox_sync_behavior COMMAND appmodel_listbox_textbox_sync_test)
add_test(NAME appmodel_layout_behavior COMMAND appmodel_layout_test)
add_test(NAME appmodel_layout_measurement_behavior COMMAND appmodel_layout_measurement_test)
add_test(NAME appmodel_menu_model_behavior COMMAND appmodel_menu_model_test)
add_test(NAME appmodel_dialog_model_behavior COMMAND appmodel_dialog_model_test)
add_test(NAME appmodel_file_api_behavior COMMAND appmodel_file_api_test)
add_test(NAME appmodel_clipboard_behavior COMMAND appmodel_clipboard_test)
add_test(NAME appmodel_text_box_behavior COMMAND appmodel_text_box_test)
add_test(NAME appmodel_text_area_behavior COMMAND appmodel_text_area_test)
add_test(NAME appmodel_focus_model_behavior COMMAND appmodel_focus_model_test)
add_test(NAME appmodel_file_drop_model_behavior COMMAND appmodel_file_drop_model_test)
add_test(NAME appmodel_file_drop_native_behavior COMMAND appmodel_file_drop_native_test)
add_test(NAME appmodel_tooltip_statusbar_behavior COMMAND appmodel_tooltip_statusbar_test)
add_test(NAME appmodel_timer_behavior COMMAND appmodel_timer_test)
add_test(NAME appmodel_progress_bar_model_behavior COMMAND appmodel_progress_bar_model_test)
add_test(NAME appmodel_progress_bar_lifecycle_behavior COMMAND appmodel_progress_bar_lifecycle_test)

if(WIN32)
    add_test(
        NAME hello_app_gui_smoke
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/gui_smoke.ps1"
            -Executable "$<TARGET_FILE:HelloApp>"
    )
    set_tests_properties(hello_app_gui_smoke PROPERTIES TIMEOUT 60)

    add_test(
        NAME multi_window_gui_smoke
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/multi_window_gui_smoke.ps1"
            -Executable "$<TARGET_FILE:MultiWindowApp>"
    )
    set_tests_properties(multi_window_gui_smoke PROPERTIES TIMEOUT 120)

    add_test(
        NAME text_input_gui_smoke
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/text_input_gui_smoke.ps1"
            -Executable "$<TARGET_FILE:TextInputApp>"
            -Cycles 3
    )
    set_tests_properties(text_input_gui_smoke PROPERTIES TIMEOUT 240)

    add_test(
        NAME list_box_gui_smoke
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/listbox_gui_smoke.ps1"
            -Executable "$<TARGET_FILE:ListBoxApp>"
            -Cycles 3
    )
    set_tests_properties(list_box_gui_smoke PROPERTIES TIMEOUT 300)

    add_test(
        NAME choice_controls_gui_smoke
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/choice_controls_gui_smoke.ps1"
            -Executable "$<TARGET_FILE:ChoiceControlsApp>"
            -Cycles 3
    )
    set_tests_properties(choice_controls_gui_smoke PROPERTIES TIMEOUT 300)

    add_test(
        NAME combo_box_gui_smoke
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/combobox_gui_smoke.ps1"
            -Executable "$<TARGET_FILE:ComboBoxApp>"
            -Cycles 3
    )
    set_tests_properties(combo_box_gui_smoke PROPERTIES TIMEOUT 360)

    add_test(
        NAME profile_manager_gui_smoke
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/profile_manager_gui_smoke.ps1"
            -Executable "$<TARGET_FILE:ProfileManagerApp>"
            -Cycles 3
    )
    set_tests_properties(profile_manager_gui_smoke PROPERTIES TIMEOUT 360)

    add_test(
        NAME profile_manager_close_gui_smoke
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/profile_manager_close_gui_smoke.ps1"
            -Executable "$<TARGET_FILE:ProfileManagerApp>"
            -Cycles 3
    )
    set_tests_properties(profile_manager_close_gui_smoke PROPERTIES TIMEOUT 600)

    add_test(
        NAME menu_gui_smoke
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/menu_gui_smoke.ps1"
            -Executable "$<TARGET_FILE:MenuApp>"
            -Cycles 3
    )
    set_tests_properties(menu_gui_smoke PROPERTIES TIMEOUT 240)

    add_test(
        NAME dialog_app_gui_smoke
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/dialog_app_gui_smoke.ps1"
            -Executable "$<TARGET_FILE:DialogApp>"
            -Cycles 3
    )
    set_tests_properties(dialog_app_gui_smoke PROPERTIES TIMEOUT 600)

    add_test(
        NAME clipboard_gui_smoke
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/clipboard_gui_smoke.ps1"
            -Executable "$<TARGET_FILE:ClipboardApp>"
            -Cycles 5
    )
    set_tests_properties(clipboard_gui_smoke PROPERTIES TIMEOUT 300)

    add_test(
        NAME text_editing_gui_smoke
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/text_editing_gui_smoke.ps1"
            -Executable "$<TARGET_FILE:TextEditingApp>"
            -Cycles 5
    )
    set_tests_properties(text_editing_gui_smoke PROPERTIES TIMEOUT 600)

    add_test(
        NAME multiline_text_gui_smoke
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/multiline_text_gui_smoke.ps1"
            -Executable "$<TARGET_FILE:MultilineTextApp>"
            -Cycles "$<$<CONFIG:Debug>:5>$<$<NOT:$<CONFIG:Debug>>:5>"
    )
    set_tests_properties(multiline_text_gui_smoke PROPERTIES TIMEOUT 300)

    add_test(
        NAME focus_command_gui_smoke
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/focus_command_gui_smoke.ps1"
            -Executable "$<TARGET_FILE:FocusCommandApp>"
            -Cycles "$<$<CONFIG:Debug>:5>$<$<NOT:$<CONFIG:Debug>>:3>"
    )
    set_tests_properties(focus_command_gui_smoke PROPERTIES TIMEOUT 600)

    add_test(
        NAME file_drop_gui_smoke
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/file_drop_gui_smoke.ps1"
            -Executable "$<TARGET_FILE:FileDropApp>"
            -Cycles "$<$<CONFIG:Debug>:5>$<$<NOT:$<CONFIG:Debug>>:3>"
    )
    set_tests_properties(file_drop_gui_smoke PROPERTIES TIMEOUT 600)

    add_test(
        NAME polish_gui_smoke
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/polish_gui_smoke.ps1"
            -Executable "$<TARGET_FILE:PolishApp>"
            -Cycles "$<$<CONFIG:Debug>:5>$<$<NOT:$<CONFIG:Debug>>:3>"
    )
    set_tests_properties(polish_gui_smoke PROPERTIES TIMEOUT 360)

    add_test(
        NAME timer_gui_smoke
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/timer_gui_smoke.ps1"
            -Executable "$<TARGET_FILE:TimerApp>"
            -Cycles 5
    )
    set_tests_properties(timer_gui_smoke PROPERTIES TIMEOUT 180)

    add_test(
        NAME progress_bar_gui_smoke
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/progress_bar_gui_smoke.ps1"
            -Executable "$<TARGET_FILE:ProgressBarApp>"
            -Cycles 5
    )
    set_tests_properties(progress_bar_gui_smoke PROPERTIES TIMEOUT 300)
endif()
