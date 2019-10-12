#include "main.h"

static void dokey(rfbBool down, rfbKeySym key, rfbClientPtr cl) {
    //fprintf(stderr, "INFO: Key down: %d\n", key);
}

int vnc_init(app_state_t *state) {
    int argc = 0;
    char *argv = "\0";
	rfbScreenInfoPtr server = state->vnc.server = rfbGetScreen(&argc,
        &argv, 
        state->video_width, 
        state->video_height, 
        5, 3, state->video_bytes_per_pixel);

	if (!state->vnc.server) {
        fprintf(stderr, "ERROR: Can't create VNC Server.\n");
		return -1;
    }

	server->desktopName = APP_NAME;
	server->frameBuffer = (char *)malloc(state->video_width * state->video_height * state->video_bytes_per_pixel);
	server->alwaysShared = 1;
	server->kbdAddEvent = dokey;

    if (server->frameBuffer == NULL) {
        fprintf(stderr, "ERROR: Can't alocatate memory for VNC Server frame.\n");
        return -1;
    }    

	fprintf(stderr, "INFO: VNC Server bpp:%d\n", server->serverFormat.bitsPerPixel);
	fprintf(stderr, "INFO: VNC Server bigEndian:%d\n", server->serverFormat.bigEndian);
	fprintf(stderr, "INFO: VNC Server redShift:%d\n", server->serverFormat.redShift);
	fprintf(stderr, "INFO: VNC Server blueShift:%d\n", server->serverFormat.blueShift);
	fprintf(stderr, "INFO: VNC Server greeShift:%d\n", server->serverFormat.greenShift);

	// Initialize the server
	rfbInitServer(server);
    return 0; 
}

int vnc_process(app_state_t *state, char* buffer, int length) {
    if (rfbIsActive(state->vnc.server)) {
        memcpy(state->vnc.server->frameBuffer, buffer, length);
        rfbMarkRectAsModified(state->vnc.server, 0, 0, state->video_width, state->video_height);
        rfbProcessEvents(state->vnc.server, 0);
    }
    return 0;
}

int vnc_destroy(app_state_t *state) {
    return 0;
}