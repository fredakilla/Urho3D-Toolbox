
set(IMGUI_HEADERS
	imconfig.h
	imgui.h
	imgui_internal.h
)

set(IMGUI_SRC
	imgui.cpp
	imgui_demo.cpp
	imgui_draw.cpp
	imgui_widgets.cpp
)

add_library(imgui STATIC ${IMGUI_SRC})
target_compile_definitions(imgui
    PUBLIC -DIMGUI_DISABLE_OBSOLETE_FUNCTIONS=1
           -DIMGUI_DEFINE_MATH_OPERATORS=1
)
target_include_directories(imgui PUBLIC .)


