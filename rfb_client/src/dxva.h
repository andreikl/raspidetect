#ifndef DXVA_H
#define DXVA_H

void dxva_destroy();
int dxva_init();

int dxva_decode(int start, int end);

#endif // DXVA_H