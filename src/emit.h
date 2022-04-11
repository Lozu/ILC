#ifndef EMIT_H
#define EMIT_H

struct function;

void emit_gn_specs(struct gn_sym_tbl *tb);
void asm_emit(struct function *f);

#endif
