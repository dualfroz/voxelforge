CORE_SRC := $(wildcard core/src/*.cpp)
TEST_SRC := $(wildcard tests/*.cpp)
RENDER_SRC := $(wildcard render/*.cpp)

CORE_INCLUDE := -Icore/include
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra $(CORE_INCLUDE)

BUILD := build
TEST_BIN := $(BUILD)/voxelforgeTests
WEB_OUT := web/index.html

.PHONY: all test web clean run-web

all: test

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(CORE_SRC) $(TEST_SRC) | $(BUILD)
	$(CXX) $(CXXFLAGS) -Itests $(CORE_SRC) $(TEST_SRC) -o $@

$(BUILD):
	mkdir -p $(BUILD)

web: $(WEB_OUT)

$(WEB_OUT): $(CORE_SRC) $(RENDER_SRC)
	@mkdir -p web
	em++ -std=c++17 -O2 $(CORE_INCLUDE) -Irender $(CORE_SRC) $(RENDER_SRC) \
		-s USE_WEBGL2=1 -s FULL_ES3=1 -s MIN_WEBGL_VERSION=2 -s MAX_WEBGL_VERSION=2 \
		-s ALLOW_MEMORY_GROWTH=1 -s EXPORTED_RUNTIME_METHODS=['UTF8ToString','stringToUTF8','lengthBytesUTF8'] \
		--shell-file render/shell.html \
		-o $(WEB_OUT)

run-web: web
	emrun --no_browser --port 8080 web/index.html

clean:
	rm -rf $(BUILD) web/index.html web/index.js web/index.wasm
