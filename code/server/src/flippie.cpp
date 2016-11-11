#include "flippie.h"

Flippie::Flippie() {}

Flippie::Flippie(flippie_t f) {
   unsigned int i;
   
   config = f;
   
   // initialze shift-register as NUMBER_OF_SHIFT_REGISTERS * 8 (unsigned char)
   _shift_register = (byte *)calloc(NUMBER_OF_SHIFT_REGISTERS, sizeof(byte));
   
   // shift-register LED_A, LED_B and LED_C
   led_A_on = false;
   led_B_on = false;
   led_C_on = false;
   
   // define pins (not srPins) as outputs
   pinMode(config.output_enable_pin, OUTPUT);
   pinMode(config.clock_pin, OUTPUT);
   pinMode(config.serial_data_pin, OUTPUT);
   pinMode(config.latch_pin, OUTPUT);
   pinMode(config.clear_pin, OUTPUT);
   pinMode(config.enable_pin, OUTPUT);
   // don't enable
   digitalWrite(config.enable_pin, HIGH);
   // enable +VS
   digitalWrite(config.vs_enable_pin, LOW);
   
   if(config.verbose)
      Serial.printf("Finished defining all output pins.\n");
   
   // reset shift-register
   digitalWrite(config.clear_pin, LOW);
   delayMicroseconds(SHIFT_REGISTER_CLEAR_TIME_IN_USEC);
   digitalWrite(config.clear_pin, HIGH);
   
   // init shift-register to tristate all outputs
   digitalWrite(config.output_enable_pin, HIGH);
   digitalWrite(config.clock_pin, LOW);
   digitalWrite(config.serial_data_pin, LOW);
   digitalWrite(config.latch_pin, LOW);
   digitalWrite(config.clear_pin, LOW);
   
   // init shift-register to ALL-LOW and just flash LED_A, LED_B and LED_C three times
   for(i = 0; i < 3; i++) {
      led_A_on = true;
      led_B_on = true;
      led_C_on = true;
      fire_shift_register(false);
      delay(100);
      led_A_on = false;
      led_B_on = false;
      led_C_on = false;
      fire_shift_register(false);
      delay(700);
   }

   if(config.verbose)
      Serial.printf("Finished initilizing shift-register.\n");
   
   // initialze _dots and _next_dots
   // array of rows => array of modules => columns are encoded bit-wise in an integer (32-bit wide => 32 column max)
   _dots = (unsigned int **)malloc(config.num_rows * sizeof(unsigned int *));
   _next_dots = (unsigned int **)malloc(config.num_rows * sizeof(unsigned int *));
   for(unsigned int i = 0; i < config.num_rows; ++i) {
      _dots[i] = (unsigned int *)calloc(config.num_modules, sizeof(unsigned int));
      _next_dots[i] = (unsigned int *)calloc(config.num_modules, sizeof(unsigned int));
   }
   
   // init fast bit-wise comparator array (int-sized)
   _int_bit_array = (unsigned int *)malloc(32 * sizeof(unsigned int));
   for(i = 0; i < 32; ++i) {
      _int_bit_array[i] = 1<<i;
   }
   // init fast bit-wise comparator array (byte-sized)
   _byte_bit_array = (byte *)malloc(8 * sizeof(byte));
   for(i = 0; i < 8; ++i) {
      _byte_bit_array[i] = 1<<i;
   }
}


// copy current state _next_dots to _dots
void Flippie::cycle_dots() {
   memcpy(_dots, _next_dots, config.num_rows * sizeof(unsigned int *));
   for(unsigned int i = 0; i < config.num_rows; ++i) {
      memcpy(_dots[i], _next_dots[i], config.num_modules * sizeof(unsigned int));
   }
}

// paint content of _next_dots
// only paint differnces to _dots unless override is true (default = false)
void Flippie::paint(bool override_former_dot_state) {
   for(unsigned int i = 0; i < config.num_rows; ++i) {
      for(unsigned int j = 0; j < config.num_modules; ++j) {
         for(unsigned int k = 0; k < config.num_columns[j]; ++k) {
            if(override_former_dot_state || (_dots[i][j] & _int_bit_array[k]) != (_next_dots[i][j] & _int_bit_array[k])) {
               set_shift_register_and_fire(i, j, k, (_next_dots[i][j] & _int_bit_array[k]) == _int_bit_array[k] ? 1 : 0);
            }
         }
      }
   }
   // _next_dots are the new _dots
   cycle_dots();
   if(config.verbose)
      Serial.printf("Done painting _next_dots.\n");
}
void Flippie::paint() {
   paint(false);
}
// set content of _next_dots and paint (cleverly only setting changed dots)
void Flippie::paint(unsigned int ** dots) {
   memcpy(_next_dots, dots, config.num_rows * sizeof(unsigned int *));
   for(unsigned int i = 0; i < config.num_rows; ++i) {
      memcpy(_next_dots[i], dots[i], config.num_modules * sizeof(unsigned int));
   }
   paint();
}

// repaint (magnetize) the whole display
void Flippie::magnetize(unsigned int repeats) {
   for(unsigned int r = 0; r < repeats; r++) {
      for(unsigned int i = 0; i < config.num_rows; ++i) {
         for(unsigned int j = 0; j < config.num_modules; ++j) {
            for(unsigned int k = 0; k < config.num_columns[j]; ++k) {
               set_shift_register_and_fire(i, j, k, _dots[i][j]);
            }
         }
      }
   }
   // _next_dots are the new _dots
   cycle_dots();
   if(config.verbose)
      Serial.printf("Done magnetizing display %d times.\n", repeats);
}


// clear all dots and paint
void Flippie::clear() {
   for(unsigned int i = 0; i < config.num_rows; ++i) {
      for(unsigned int j = 0; j < config.num_modules; ++j) {
         _next_dots[i][j] = 0;
         _dots[i][j] = 0;
         for(unsigned int k = 0; k < config.num_columns[j]; ++k) {
            set_shift_register_and_fire(i, j, k, 0);
         }
      }
   }
   if(config.verbose)
      Serial.printf("Done clearing display.\n");
}

// calc inverse of _dots, save to _next_dots and paint
void Flippie::inverse() {
   unsigned int x = 0;
   for(unsigned int i = 0; i < config.num_rows; ++i) {
      for(unsigned int j = 0; j < config.num_modules; ++j) {
         _next_dots[i][j] = UINT_MAX - _dots[i][j];
         for(unsigned int k = 0; k < config.num_columns[j]; ++k) {
            set_shift_register_and_fire(i, j, k, _next_dots[i][j]);
         }
      }
   }
   if(config.verbose)
      Serial.printf("Done inversing display.\n");
}

// fill all dots, save to _next_dots and paint
void Flippie::fill() {
   unsigned int x = 0;
   for(unsigned int i = 0; i < config.num_rows; ++i) {
      for(unsigned int j = 0; j < config.num_modules; ++j) {
         _next_dots[i][j] = UINT_MAX;
         _dots[i][j] = UINT_MAX;
         for(unsigned int k = 0; k < config.num_columns[j]; ++k) {
            set_shift_register_and_fire(i, j, k, _next_dots[i][j]);
         }
      }
   }
   if(config.verbose)
      Serial.printf("Done filling display.\n");
}

// set/get row_set
void Flippie::set_row_set(unsigned int row) {
   // reset all RST rows
   for(unsigned int i = 0; i < config.num_rows; ++i) {
      _shift_register[config.sr_row_rst_pins[i]/8] &= (255 - _byte_bit_array[config.sr_row_rst_pins[i]%8]);
   }
   _shift_register[config.sr_row_set_pins[row]/8] |= _byte_bit_array[config.sr_row_set_pins[row]%8];
}
int Flippie::get_row_set() {
   for(unsigned int row = 0; row < BROSE_MAX_ROWS; row++) {
      if((_shift_register[config.sr_row_set_pins[row]/8] & _byte_bit_array[config.sr_row_set_pins[row]%8]) == _byte_bit_array[config.sr_row_set_pins[row]%8])
         return row;
   }
   return -1;
}

// set/get row_rst
void Flippie::set_row_rst(unsigned int row) {
   // reset all SET rows
   for(unsigned int i = 0; i < config.num_rows; ++i) {
      _shift_register[config.sr_row_set_pins[i]/8] &= (255 - _byte_bit_array[config.sr_row_set_pins[i]%8]);
   }
   _shift_register[config.sr_row_rst_pins[row]/8] |= _byte_bit_array[config.sr_row_rst_pins[row]%8];
}
int Flippie::get_row_rst() {
   for(unsigned int row = 0; row < BROSE_MAX_ROWS; row++) {
      if((_shift_register[config.sr_row_rst_pins[row]/8] & _byte_bit_array[config.sr_row_rst_pins[row]%8]) == _byte_bit_array[config.sr_row_rst_pins[row]%8])
         return row;
   }
   return -1;
}

// set/get coolumn
void Flippie::set_column(unsigned int column) {
   for(unsigned int i = 0; i < FP2800A_COLUMN_CODE_LINES; i++) {
      if(FP2800A_COLUMN_CODES[column][i] == 1) {
         _shift_register[config.sr_column_code_pins[i]/8] |= _byte_bit_array[config.sr_column_code_pins[i]%8];
      } else {
         _shift_register[config.sr_column_code_pins[i]/8] &= (255 - _byte_bit_array[config.sr_column_code_pins[i]%8]);
      }
   }
}
int Flippie::get_column() {
   bool found = true;
   for(unsigned int column = 0; column < FP2800A_MAX_COLUMNS; column++) {
      found = true;
      for(unsigned int i = 0; i < FP2800A_COLUMN_CODE_LINES; i++) {
         found &= ((_shift_register[config.sr_column_code_pins[i]/8] & _byte_bit_array[config.sr_column_code_pins[i]%8]) == _byte_bit_array[config.sr_column_code_pins[i]%8]);
      }
      if(found)
         return column;
   }
   return -1;
}

// set/get address (ADDR1-ADDR7)
void Flippie::set_address(byte address) {
   for(unsigned int i = 0; i < BROSE_ADDR_LINES - 1; ++i) {
      if((address>>i) & 1 == 1) {
         _shift_register[config.sr_address_pins[i]/8] |= _byte_bit_array[config.sr_address_pins[i]%8];
      }
   }
}
byte Flippie::get_address() {
   byte address = 0x00;
   for(int i = BROSE_ADDR_LINES - 2; i >= 0 ; i--) {
      if((_shift_register[config.sr_address_pins[i]/8] & _byte_bit_array[config.sr_address_pins[i]%8]) == _byte_bit_array[config.sr_address_pins[i]%8]) {
         address |= 1;
      }
      address<<1;
   }
   return address;
}

// set FP2800A direction 1 => switches +VS, 0 switches GND
void Flippie::set_d(unsigned int state) {
   if(state == 1) {
      _shift_register[config.sr_d_pin/8] |= _byte_bit_array[config.sr_d_pin%8];
   } else {
      _shift_register[config.sr_d_pin/8] &= (255 - _byte_bit_array[config.sr_d_pin%8]);
   }
}
unsigned int Flippie::get_d() {
   return ((_shift_register[config.sr_d_pin/8] & _byte_bit_array[config.sr_d_pin%8]) == _byte_bit_array[config.sr_d_pin%8]) ? 1 : 0;
}

// clear shift-registers
void Flippie::clear_shift_register(bool fire_after_clear) {
   for(unsigned int i = 0; i < NUMBER_OF_SHIFT_REGISTERS; ++i) {
      _shift_register[i] = 0;
   }
   // fireing shift-register if requested
   if(fire_after_clear) {
      fire_shift_register(false);
   }
}

// assemble the shift-register for a specific dotsand fire it
// resetting shift-register and set all output to low afterwards
void Flippie::set_shift_register_and_fire(unsigned int row, unsigned int module, unsigned int column, unsigned int state) {
   unsigned int i;
   
   // clear shift-register
   for(i = 0; i < NUMBER_OF_SHIFT_REGISTERS; ++i) {
      _shift_register[i] = 0;
   }
   
   // set state of the selected row
   if(state == 1) {
      _shift_register[config.sr_row_set_pins[row]/8] |= _byte_bit_array[config.sr_row_set_pins[row]%8];
   } else {
      _shift_register[config.sr_row_rst_pins[row]/8] |= _byte_bit_array[config.sr_row_rst_pins[row]%8];
   }
   
   // set column code (B1-A0)
   for(i = 0; i < FP2800A_COLUMN_CODE_LINES; ++i) {
      if(FP2800A_COLUMN_CODES[(FP2800A_MAX_COLUMNS - config.num_columns[module]) + column][i] == 1) {
         _shift_register[config.sr_column_code_pins[i]/8] |= _byte_bit_array[config.sr_column_code_pins[i]%8];
      }
   }
   
   // set address (ADDR1-ADDR7)
   for(i = 0; i < BROSE_ADDR_LINES - 1; ++i) {
      if(config.addresses[module]>>i & 1 == 1) {
         _shift_register[config.sr_address_pins[i]/8] |= _byte_bit_array[config.sr_address_pins[i]%8];
      }
   }
   
   // set D
   if(state == 0) {
      _shift_register[config.sr_d_pin/8] |= _byte_bit_array[config.sr_d_pin%8];
   }

   // set LEDs
   // if configured flash automatically
   if(config.led_mode == LED_MODE_FLASHING) {
      led_C_on = !led_C_on;
   }
   if(led_A_on) {
      _shift_register[config.sr_led_a_pin/8] |= _byte_bit_array[config.sr_led_a_pin%8];
   }
   if(led_B_on) {
      _shift_register[config.sr_led_b_pin/8] |= _byte_bit_array[config.sr_led_b_pin%8];
   }
   if(led_C_on) {
      _shift_register[config.sr_led_c_pin/8] |= _byte_bit_array[config.sr_led_c_pin%8];
   }

   // firing shift-register
   fire_shift_register(true);
}

// fire a given shift-register according to pins given
void Flippie::fire_shift_register(bool enable) {
   // just in case => defined start point
   digitalWrite(config.clock_pin, LOW);
   // just in case => clear shift-register
   digitalWrite(config.clear_pin, LOW);
   digitalWrite(config.clear_pin, HIGH);
   
   // shift out (reverse order) shift-register byte array
   for(unsigned int i = NUMBER_OF_SHIFT_REGISTERS * 8; 0 < i--;) {
      digitalWrite(config.serial_data_pin, (_shift_register[i/8] & _byte_bit_array[i%8]) == _byte_bit_array[i%8] ? HIGH : LOW);
      digitalWrite(config.clock_pin, HIGH);
      digitalWrite(config.clock_pin, LOW);
   }
   digitalWrite(config.latch_pin, HIGH);
   digitalWrite(config.latch_pin, LOW);
   
   if(enable) {
      // fire using ADDR8 aka NOT_EN => FP2800A enable (E)
      digitalWrite(config.enable_pin, HIGH);
      delayMicroseconds(FP2800A_ACTIVE_TIME_IN_USEC);
      digitalWrite(config.enable_pin, LOW);
   }

   if(config.verbose)
      Serial.printf(enable ? "*" : ".");

   yield();
}

// fire a given shift-register according to pins given
// print shift-register
String Flippie::shift_register_as_string() {
   char *tmp = new char[(3 * ((NUMBER_OF_SHIFT_REGISTERS * 8) + 1)) + 1];
   unsigned int tmp_pos = 0;
   
   tmp_pos += sprintf(tmp + tmp_pos, "SRA: ");
   for(unsigned int i = NUMBER_OF_SHIFT_REGISTERS * 8; 0 < i--;) {
      tmp_pos += sprintf(tmp + tmp_pos, "%hhu", i/8);
   }

   tmp_pos += sprintf(tmp + tmp_pos, "\nSRA: ");
   for(unsigned int i = NUMBER_OF_SHIFT_REGISTERS * 8; 0 < i--;) {
      tmp_pos += sprintf(tmp + tmp_pos, "%hhu", i%8);
   }
   
   tmp_pos += sprintf(tmp + tmp_pos, "\nSRD: ");
   for(unsigned int i = NUMBER_OF_SHIFT_REGISTERS * 8; 0 < i--;) {
      tmp[tmp_pos] = (_shift_register[i/8] & _byte_bit_array[i%8]) == _byte_bit_array[i%8] ? 'H' : 'L';
      tmp_pos++;
   }
   sprintf(tmp + tmp_pos, "\n\0");
   return String(tmp);
}

// return shift-register in shortend form as json string
String Flippie::shift_register_as_json_short_string() {
   char *bit_string = (char *)malloc(((NUMBER_OF_SHIFT_REGISTERS * sizeof(byte)) + 3) * sizeof(char));
   bit_string[0] = '"';
   unsigned int l = 1;
   for(unsigned int i = 0; i < NUMBER_OF_SHIFT_REGISTERS; i++) {
      for(unsigned int j = 0; j < sizeof(byte); j++) {
         bit_string[l] = ((((_shift_register[i]<<j) & 1) == 1) ? '1' : '0');
         l++;
      }
   }
   bit_string[l] = '"';
   bit_string[l + 1] = '\0';
   return String(bit_string);
}



// // set all possible shift-register outputs (filling the shift-register and fire it)
// void Flippie::test(unsigned int test_bit, unsigned int state) {
//    if(state == 0) {
//       _shift_register[test_bit/8] &= (255 - _byte_bit_array[test_bit%8]);
//    } else {
//       _shift_register[test_bit/8] |= _byte_bit_array[test_bit%8];
//    }
//    Serial.printf("set bit-position %u(%u,%u) to %u => [ %u", test_bit, test_bit/8, test_bit%8, state, _shift_register[0]);
//    for(unsigned int i = 1; i < NUMBER_OF_SHIFT_REGISTERS; ++i) {
//       Serial.printf(", %u", _shift_register[i]);
//    }
//    Serial.println("]");
//    fire_shift_register_with_print(false);
// }

