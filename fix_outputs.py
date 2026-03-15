import sys

with open('src/uart_handler.c', 'r', encoding='utf-8') as f:
    text = f.read()

# Fix 1: list
old_list = '''            if (leafts_get_by_index(db, record_index, &record) == LEAFTS_OK)
            {
                snprintf(response, sizeof(response),
                         "%lu %f\\n", (unsigned long)record.timestamp, record.value);
                uart_send_str(uart, response);
            }'''
new_list = '''            if (leafts_get_by_index(db, record_index, &record) == LEAFTS_OK)
            {
                if (record.magic == LEAFTS_MAGIC_TEXT) {
                    snprintf(response, sizeof(response),
                             "%lu %.4s\\n", (unsigned long)record.timestamp, record.text);
                } else {
                    snprintf(response, sizeof(response),
                             "%lu %f\\n", (unsigned long)record.timestamp, record.value);
                }
                uart_send_str(uart, response);
            }'''
text = text.replace(old_list, new_list)

# Fix 2: select *
old_sel = '''                // Standard row output
                snprintf(response, sizeof(response), "%lu %f\\n", (unsigned long)record.timestamp, record.value);
                uart_send_str(uart, response);'''
new_sel = '''                // Standard row output
                if (record.magic == LEAFTS_MAGIC_TEXT) {
                    snprintf(response, sizeof(response), "%lu %.4s\\n", (unsigned long)record.timestamp, record.text);
                } else {
                    snprintf(response, sizeof(response), "%lu %f\\n", (unsigned long)record.timestamp, record.value);
                }
                uart_send_str(uart, response);'''
text = text.replace(old_sel, new_sel)

old_latest = '''snprintf(response, sizeof(response), "OK %lu %f\\n", (unsigned long)record.timestamp, record.value);'''
new_latest = '''if (record.magic == LEAFTS_MAGIC_TEXT) {
        snprintf(response, sizeof(response), "OK %lu %.4s\\n", (unsigned long)record.timestamp, record.text);
    } else {
        snprintf(response, sizeof(response), "OK %lu %f\\n", (unsigned long)record.timestamp, record.value);
    }'''
text = text.replace(old_latest, new_latest)

with open('src/uart_handler.c', 'w', encoding='utf-8') as f:
    f.write(text)

