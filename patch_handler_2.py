import sys
import re

with open('c:/Projects/LeafTS/src/uart_handler.c', 'r', encoding='utf-8') as f:
    text = f.read()

# For any block doing if (leafts_get_by_index(db, idx, &VAR) == LEAFTS_OK)
# We can inject if (VAR.magic == LEAFTS_MAGIC_TEXT) continue;
# BUT wait! Not all of them are inside loops. Some return, some skip.
# Let's just do a specific replace for the aggregate block inside select *
old_agg = r'''                if \(agg_type == AGG_MIN\) \{
                    if \(record\.payload\.value < min_val\) \{
                        min_val \= record\.payload\.value;'''

new_agg = r'''                if (record.magic == LEAFTS_MAGIC_TEXT) continue;
                if (agg_type == AGG_MIN) {
                    if (record.payload.value < min_val) {
                        min_val = record.payload.value;'''
text = re.sub(old_agg, new_agg, text)

with open('c:/Projects/LeafTS/src/uart_handler.c', 'w', encoding='utf-8') as f:
    f.write(text)
print("Replaced agg")
