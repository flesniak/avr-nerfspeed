SRC = nerfspeed.c
#INC_SRC = hd44780.c

F_CPU = 8000000
MCU = atmega8
CFLAGS = -s -Os -Wall -Werror -std=gnu99 -DF_CPU=$(F_CPU) -mmcu=$(MCU)
AVRDUDE_MCU = m8
AVRDUDE_PROG = usbasp

HFUSE = 0xd9
LFUSE = 0xf4

BIN = $(SRC:%.c=%)
HEX = $(BIN).hex
INC_OBJ = $(INC_SRC:%.c=%.o)

.PHONY: all flash fuses clean

all: $(HEX)

$(BIN): $(SRC) $(INC_OBJ)
	avr-gcc $(CFLAGS) -o $(BIN) $^

$(INC_OBJ): $(INC_SRC)
	avr-gcc $(CFLAGS) -c -o $@ $^

$(HEX): $(BIN)
	avr-objcopy -O ihex $(BIN) $(HEX)

flash: $(HEX)
	avrdude -p $(AVRDUDE_MCU) -c $(AVRDUDE_PROG) -eU flash:w:$(HEX)

fuses:
	avrdude -p $(AVRDUDE_MCU) -c $(AVRDUDE_PROG) -U lfuse:w:$(LFUSE):m -U hfuse:w:$(HFUSE):m

clean:
	rm -f $(BIN) $(HEX) $(INC_OBJ)
