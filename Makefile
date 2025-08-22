NAME=webserv

UNAME=$(shell uname)

CFLAGS=-Wall -Wextra -Werror -std=c++98
CFLAGS_DEBUG = $(CFLAGS) -fsanitize=address -fsanitize=undefined
AR = ar -rc
ifeq ($(UNAME),Linux)
	LFLAGS = -L/usr/lib
	# EXT_INCLUDE = /usr/include
	EXT_DEFINES = -D LINUX
	CC = g++
else
	# LFLAGS = -lm -Lmlx -lmlx -framework OpenGL -framework AppKit
	# EXT_INCLUDE = ""
	EXT_DEFINES = -D OSX
	CC = c++
endif

# source and object directories
SRC_ROOT = src
OBJ_ROOT = obj
DBG_OBJ_ROOT = dobj
SRC_DIR = $(sort $(SRC_ROOT)/ $(dir $(wildcard $(SRC_ROOT)/*/)))
OBJ_DIR = $(patsubst $(SRC_ROOT)/%,$(OBJ_ROOT)/%,$(SRC_DIR))
DBG_OBJ_DIR = $(patsubst $(SRC_ROOT)/%,$(DBG_OBJ_ROOT)/%,$(SRC_DIR))

$(info $$SRC_DIR is [${SRC_DIR}])
$(info $$OBJ_DIR is [${OBJ_DIR}])

# object files
SRC = $(wildcard $(addsuffix *.cpp, $(SRC_DIR)))
OBJ = $(patsubst $(SRC_ROOT)/%.cpp,$(OBJ_ROOT)/%.o,$(SRC))
DBG_OBJ = $(patsubst $(SRC_ROOT)/%.cpp,$(DBG_OBJ_ROOT)/%.o,$(SRC))

$(info $$SRC is [${SRC}])
$(info $$OBJ is [${OBJ}])

# defines and includes
DEFINES = $(EXT_DEFINES)
INC_DIR = -I ./ $(EXT_INCLUDE)

all: $(NAME)

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

$(DBG_OBJ_DIR):
	@mkdir -p $(DBG_OBJ_DIR)

$(NAME): $(OBJ)
	@echo $(NAME): creating binary $(NAME) for platform $(UNAME)
	$(CC) -o $(NAME) $(OBJ) $(LFLAGS)

$(OBJ_ROOT)/%.o : $(SRC_ROOT)/%.cpp | $(OBJ_DIR)
	@echo $(NAME): building $@
	@$(CC) $(CFLAGS) $(INC_DIR) $(DEFINES) -c $< -o $@

debug: $(DBG_OBJ)
	@echo $(NAME): creating debuggable binary $(NAME) for platform $(UNAME)
	@$(CC) -o $(NAME) $(DBG_OBJ) $(LFLAGS)

$(DBG_OBJ_ROOT)/%.o : $(SRC_ROOT)/%.cpp | $(DBG_OBJ_DIR)
	@echo $(NAME): debug building $@
	@$(CC) $(CFLAGS) $(INC_DIR) $(DEFINES) -D DEBUG -c $< -o $@ -g

clean:
	@echo $(NAME): cleaning objects
	@rm -f $(OBJ) $(DBG_OBJ)

fclean: clean
	@echo $(NAME): cleaning build artifacts
	@rm -f $(NAME)

re: fclean all

redb: fclean debug

.PHONY: all clean fclean re debug redb ylib ylib-debug ylib-clean ylib-fclean
