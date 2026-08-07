CXX      := clang++
CXXFLAGS := -std=c++20 -O3 -Wall -Wextra -Wno-deprecated-declarations -Wno-missing-field-initializers -Wno-deprecated

ARCH_FLAGS := -arch arm64

BREW_PREFIX := $(shell brew --prefix 2>/dev/null || echo /opt/homebrew)
EXTERNAL_DIR := external
IMGUI_DIR := $(EXTERNAL_DIR)/imgui
GLAD_DIR := $(EXTERNAL_DIR)/glad
INC_DIR := include

INCLUDES := -I$(EXTERNAL_DIR) \
            -I$(IMGUI_DIR) \
            -I$(IMGUI_DIR)/backends \
            -I$(GLAD_DIR)/include \
            -I$(BREW_PREFIX)/include \
			-I$(INC_DIR)

LDFLAGS  := -L$(BREW_PREFIX)/lib -lglfw
FRAMEWORKS := -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo -framework UniformTypeIdentifiers

IMGUI_SRCS := $(IMGUI_DIR)/imgui.cpp \
              $(IMGUI_DIR)/imgui_draw.cpp \
              $(IMGUI_DIR)/imgui_widgets.cpp \
              $(IMGUI_DIR)/imgui_tables.cpp \
              $(IMGUI_DIR)/backends/imgui_impl_glfw.cpp \
              $(IMGUI_DIR)/backends/imgui_impl_opengl3.cpp

GLAD_SRCS := $(GLAD_DIR)/src/glad.c

APP_CPP_SRCS := src/main.cpp
APP_OBJC_SRCS := src/NativeFileDialog.mm

OBJS := $(APP_CPP_SRCS:.cpp=.o) $(APP_OBJC_SRCS:.mm=.o) $(IMGUI_SRCS:.cpp=.o) $(GLAD_SRCS:.c=.o)

TARGET := ascii_studio

.PHONY: all clean fclean re

all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "Linking $(TARGET)..."
	@$(CXX) $(CXXFLAGS) $(ARCH_FLAGS) $(OBJS) $(LDFLAGS) $(FRAMEWORKS) -o $(TARGET)
	@echo "Build complete! Run with ./$(TARGET)"

%.o: %.cpp
	@echo "Compiling $<..."
	@$(CXX) $(CXXFLAGS) $(ARCH_FLAGS) $(INCLUDES) -c $< -o $@

%.o: %.mm
	@echo "Compiling $<..."
	@$(CXX) $(CXXFLAGS) $(ARCH_FLAGS) $(INCLUDES) -c $< -o $@

%.o: %.c
	@echo "Compiling $<..."
	@$(CXX) $(CXXFLAGS) $(ARCH_FLAGS) $(INCLUDES) -c $< -o $@

clean:
	@echo "Cleaning object files..."
	@rm -f $(OBJS)

fclean: clean
	@echo "Cleaning binary..."
	@rm -f $(TARGET)

re: fclean all
