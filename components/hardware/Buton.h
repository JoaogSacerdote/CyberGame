#ifndef BUTON_H
#define BUTON_H

#include <stdbool.h>

// Mapeamento dos pinos conforme o diagram.json
#define BTN_UP    15 // btn1
#define BTN_DOWN  16 // btn5
#define BTN_LEFT  17 // btn3
#define BTN_RIGHT 18 // btn4

// Declaração das funções
void botoes_init(void);
bool botao_pressionado(int pino);

#endif