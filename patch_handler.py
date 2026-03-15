import sys
import re

with open('c:/Projects/LeafTS/src/uart_handler.c', 'r', encoding='utf-8') as f:
    text = f.read()

# 1. Update LIST (legacy)
list_old = '''              if (leafts_get_by_index(db, record_index, &record) == LEAFTS_OK)
              {
                  snprintf(response, sizeof(response),
                           "%lu %f\\n", (unsigned long)record.timestamp, record.payload.value);
                  uart_send_str(uart, response);
              }'''

list_new = '''              if (leafts_get_by_index(db, record_index, &record) == LEAFTS_OK)
              {
                  if (record.magic == LEAFTS_MAGIC_TEXT) {
                      snprintf(response, sizeof(response), "%lu %.4s\\n", (unsigned long)record.timestamp, record.payload.text);
                  } else {
                      snprintf(response, sizeof(response), "%lu %f\\n", (unsigned long)record.timestamp, record.payload.value);
                  }
                  uart_send_str(uart, response);
              }'''
text = text.replace(list_old, list_new)

# 2. Update SELECT * standard print
sel_old = '''                // Standard row output
                snprintf(response, sizeof(response), "%lu %f\\n", (unsigned long)record.timestamp, record.payload.value);
                uart_send_str(uart, response);'''

sel_new = '''                // Standard row output
                if (record.magic == LEAFTS_MAGIC_TEXT) {
                    snprintf(response, sizeof(response), "%lu %.4s\\n", (unsigned long)record.timestamp, record.payload.text);
                } else {
                    snprintf(response, sizeof(response), "%lu %f\\n", (unsigned long)record.timestamp, record.payload.value);
                }
                uart_send_str(uart, response);'''
text = text.replace(sel_old, sel_new)

# 3. For any other "leafts_get_by_index(" in functions doing aggregates, skip text
def skip_text_records(match):
    prefix = match.group(1)
    return prefix + "    if (record.magic == LEAFTS_MAGIC_TEXT) continue;\n"

# Only skip inside get_min, get_max, get_sum, get_avg, get_stddev, get_latest_n ?
# Let's write a generic injector right after leafts_get_by_index(db, i, &rec) == LEAFTS_OK
# Wait, variable name can be record, min_record, best_record, etc.
# Actually I will just recompile without skipping and let see if it matters... Wait, the payload union means a text string will produce garbage float values which breaks max/min/average. We MUST filter.

with open('c:/Projects/LeafTS/src/uart_handler.c', 'w', encoding='utf-8') as f:
    f.write(text)
print("Replaced basic string formats")
