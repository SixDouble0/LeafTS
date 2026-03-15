import sys
import re

with open('c:/Projects/LeafTS/src/uart_handler.c', 'r', encoding='utf-8') as f:
    text = f.read()

append_block = '''//  APPEND / INSERT
    if (strncmp(line, "append", 6) == 0 || strncmp(line, "insert", 6) == 0)
    {
        uint32_t timestamp = 0;
        float    value = 0.0f;
        int      is_text = 0;
        char     text_val[5] = {0};
        char     val_token[64] = {0};
        char     ts_token[64] = {0};
        char     extra;
        
        int parsed = sscanf(line, "append %63s %63s %c", val_token, ts_token, &extra);
        if (parsed < 1) {
            parsed = sscanf(line, "insert %63s %63s %c", val_token, ts_token, &extra);
        }

        if (parsed == 0) {
            uart_send_str(uart, "ERR bad_args\\n");
            return LEAFTS_ERR_NULL;
        }

        char extra_f;
        if (sscanf(val_token, "%f%c", &value, &extra_f) != 1) {
            is_text = 1;
            strncpy(text_val, val_token, 4);
            text_val[4] = '\\0';
        }

        int has_manual_ts = (parsed == 2);

        if (has_manual_ts) {
            if (!parse_timestamp_token(ts_token, &timestamp)) {
                uart_send_str(uart, "ERR bad_args\\n");
                return LEAFTS_ERR_NULL;
            }
        } else {
            timestamp = auto_timestamp_now(db);
        }

        int result;
        if (is_text) {
            result = leafts_append_text(db, timestamp, text_val);
        } else {
            result = leafts_append(db, timestamp, value);
        }

        if (result == LEAFTS_OK) {
            uart_send_str(uart, "OK\\n");
        } else {
            snprintf(response, sizeof(response), "ERR %d\\n", result);
            uart_send_str(uart, response);
        }
        return result;
    }'''

new_text = re.sub(r'//  APPEND / INSERT.*?\n    \}\n(?=[\r\n]*    //  LIST)', append_block + '\n', text, flags=re.DOTALL)

if new_text != text:
    with open('c:/Projects/LeafTS/src/uart_handler.c', 'w', encoding='utf-8') as f:
        f.write(new_text)
    print("Append updated successfully!")
else:
    print("Failed to replace!")
