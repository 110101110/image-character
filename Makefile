CXX      := clang++
CXXFLAGS := -std=c++20 -O3 -Wall -Wextra \
	-Wno-deprecated-declarations -Wno-missing-field-initializers \
	-Wno-deprecated

ARCH_FLAGS := -arch arm64

BREW_PREFIX := /opt/homebrew
GLFW_PREFIX := /opt/homebrew/opt/glfw
EXTERNAL_DIR := external
IMGUI_DIR := $(EXTERNAL_DIR)/imgui
GLAD_DIR := $(EXTERNAL_DIR)/glad
INC_DIR := include
OBJ_DIR := obj

INCLUDES := -I$(EXTERNAL_DIR) \
            -I$(IMGUI_DIR) \
            -I$(IMGUI_DIR)/backends \
            -I$(GLAD_DIR)/include \
			-I$(BREW_PREFIX)/include \
			-I$(GLFW_PREFIX)/include \
			-I$(INC_DIR)

LDFLAGS  := -L$(GLFW_PREFIX)/lib -lglfw
FRAMEWORKS := -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo -framework UniformTypeIdentifiers

IMGUI_SRCS := $(IMGUI_DIR)/imgui.cpp \
              $(IMGUI_DIR)/imgui_draw.cpp \
              $(IMGUI_DIR)/imgui_widgets.cpp \
              $(IMGUI_DIR)/imgui_tables.cpp \
              $(IMGUI_DIR)/backends/imgui_impl_glfw.cpp \
              $(IMGUI_DIR)/backends/imgui_impl_opengl3.cpp

GLAD_SRCS := $(GLAD_DIR)/src/glad.c

APP_CPP_SRCS := src/main.cpp \
				src/AsciiArt.cpp \
				src/AsciiExport.cpp \
				src/FontAtlas.cpp \
				src/StbImplementation.cpp
APP_OBJC_SRCS := src/NativeFileDialog.mm

CPP_SRCS := $(APP_CPP_SRCS) $(IMGUI_SRCS)
CPP_OBJS := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(CPP_SRCS))
OBJC_OBJS := $(patsubst %.mm,$(OBJ_DIR)/%.o,$(APP_OBJC_SRCS))
C_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(GLAD_SRCS))
OBJS := $(CPP_OBJS) $(OBJC_OBJS) $(C_OBJS)

TARGET := image_character

.PHONY: all clean fclean re

all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "Linking $(TARGET)..."
	@$(CXX) $(CXXFLAGS) $(ARCH_FLAGS) $(OBJS) $(LDFLAGS) $(FRAMEWORKS) -o $(TARGET)
	@echo "Build complete! Run with ./$(TARGET)"

$(OBJ_DIR)/%.o: %.cpp
	@echo "Compiling $<..."
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(ARCH_FLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/%.o: %.mm
	@echo "Compiling $<..."
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(ARCH_FLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/%.o: %.c
	@echo "Compiling $<..."
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(ARCH_FLAGS) $(INCLUDES) -c $< -o $@

clean:
	@echo "Cleaning object files..."
	@rm -rf $(OBJ_DIR)

fclean: clean
	@echo "Cleaning binary..."
	@rm -f $(TARGET)

re: fclean all
