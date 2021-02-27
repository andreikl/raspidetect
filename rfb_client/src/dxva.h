#ifndef DXVA_H
#define DXVA_H

void dxva_destroy(struct app_state_t *app);
int dxva_init(struct app_state_t *app);

int dxva_decode(struct app_state_t *app);

#endif // DXVA_H