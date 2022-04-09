#include <xcb/xcb.h>
#include <xcb/randr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
static const char* display_name = ":0";
static const char* xauthority = "/home/zignage/.Xauthority";
int main(void) {
    setenv("XAUTHORITY", xauthority, 1);
    xcb_connection_t* conn = xcb_connect(display_name, nullptr);
    if(xcb_connection_has_error(conn)) {
        return EXIT_FAILURE;
    }
    if(!xcb_get_extension_data(conn, &xcb_randr_id)->present) {
        return EXIT_FAILURE;
    }
    xcb_randr_query_version_cookie_t ver_c = xcb_randr_query_version(conn, XCB_RANDR_MAJOR_VERSION, XCB_RANDR_MINOR_VERSION);
    xcb_randr_query_version_reply_t* ver_r = xcb_randr_query_version_reply(conn, ver_c, 0);
    if(!ver_r) {
        return EXIT_FAILURE;
    }
    if(!(ver_r->major_version > 1 || ver_r->minor_version >= 5)) {
        return EXIT_FAILURE;
    }
    free(ver_r);
    const xcb_setup_t* setup = xcb_get_setup(conn);
    xcb_screen_t* screen = xcb_setup_roots_iterator(setup).data;
    xcb_randr_get_monitors_cookie_t mon_c = xcb_randr_get_monitors(conn, screen->root, true);
    xcb_randr_get_monitors_reply_t* mon_r = xcb_randr_get_monitors_reply(conn, mon_c, 0);
    if(!mon_r) {
        return EXIT_FAILURE;
    }
    xcb_randr_monitor_info_iterator_t it = xcb_randr_get_monitors_monitors_iterator(mon_r);
    for(int screen_num = 0; it.rem; xcb_randr_monitor_info_next(&it), screen_num++) {
        xcb_randr_monitor_info_t* mon = it.data;
	xcb_get_atom_name_cookie_t atom_c = xcb_get_atom_name(conn, mon->name);
	xcb_get_atom_name_reply_t* atom_r = xcb_get_atom_name_reply(conn, atom_c, 0);
	char* name = strndup(xcb_get_atom_name_name(atom_r), xcb_get_atom_name_name_length(atom_r));
        printf("{\"screen\":%d,\"name\":\"%s\",\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d}\n",
            screen_num, name, mon->x, mon->y, mon->width, mon->height);
	free(name);
	free(atom_r);
    }
    free(mon_r);
    xcb_randr_get_screen_resources_current_cookie_t scr_c = xcb_randr_get_screen_resources_current(conn, screen->root);
    xcb_randr_get_screen_resources_current_reply_t* scr_r = xcb_randr_get_screen_resources_current_reply(conn, scr_c, 0);
    if(!scr_r) {
        return EXIT_FAILURE;
    }
    xcb_timestamp_t ts = scr_r->config_timestamp;
    const int output_count = xcb_randr_get_screen_resources_current_outputs_length(scr_r);
    xcb_randr_output_t *outputs = xcb_randr_get_screen_resources_current_outputs(scr_r);
    for(int output_num = 0; output_num < output_count; output_num++) {
	xcb_randr_output_t id = outputs[output_num];
	xcb_randr_get_output_info_cookie_t out_c = xcb_randr_get_output_info(conn, id, ts);
	xcb_randr_get_output_info_reply_t* out_r = xcb_randr_get_output_info_reply(conn, out_c, 0);
	if(out_r == 0) continue;
	char* name = strndup((const char*)xcb_randr_get_output_info_name(out_r), xcb_randr_get_output_info_name_length(out_r));
	if(out_r->crtc == XCB_NONE || out_r->connection == XCB_RANDR_CONNECTION_DISCONNECTED)
        {
            printf("{\"output\":%d,\"name\":\"%s\",\"is_connected\":false}\n", output_num, name);
	    free(name);
            free(out_r);
            continue;
        }
	xcb_randr_get_crtc_info_cookie_t crtc_c = xcb_randr_get_crtc_info(conn, out_r->crtc, ts);
	xcb_randr_get_crtc_info_reply_t* crtc_r = xcb_randr_get_crtc_info_reply(conn, crtc_c, 0);
	if(!crtc_r) {
	    free(name);
            free(out_r);
            return EXIT_FAILURE;
	}
	xcb_intern_atom_cookie_t atom_c = xcb_intern_atom(conn, 1, 4, "EDID");
	xcb_intern_atom_reply_t* atom_r = xcb_intern_atom_reply(conn, atom_c, 0);
	if(atom_r) {
	    xcb_randr_get_output_property_cookie_t prp_c = xcb_randr_get_output_property(conn, id, atom_r->atom, XCB_GET_PROPERTY_TYPE_ANY, 0, 256, 0, 0);
	    xcb_randr_get_output_property_reply_t* prp_r = xcb_randr_get_output_property_reply(conn, prp_c, 0);
	    free(atom_r);
	    if(!prp_r) {
		free(crtc_r);
                free(name);
                free(out_r);
		continue;
	    }
	    const unsigned char *edid = xcb_randr_get_output_property_data(prp_r);
	    const int length = xcb_randr_get_output_property_data_length(prp_r);
	    if(!edid || length == 0) {
   	        free(prp_r);
		free(crtc_r);
                free(name);
                free(out_r);
		continue;
	    }
	    unsigned char sum = 0;
	    for(int i = 0; i < length; i++) {
	        sum += edid[i];
	    }
	    if(sum) {
   	        free(prp_r);
		free(crtc_r);
                free(name);
                free(out_r);
	        continue;
	    }
	    const unsigned char magic[] = { 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00 };
	    if(memcmp(magic, edid, 8)) {
   	        free(prp_r);
		free(crtc_r);
                free(name);
                free(out_r);
		continue;
	    }
            printf("{\"output\":%d,\"name\":\"%s\",\"is_connected\":true,\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d,\"edid\":\"",
                output_num, name, crtc_r->x, crtc_r->y, crtc_r->width, crtc_r->height);
            for (int k = 0; k < length; k++) {
	        printf("%02" PRIx8, edid[k]);
            }
            printf("\"}\n");
        }
        else
        {
            printf("{\"output\":%d,\"name\":\"%s\",\"is_connected\":true,\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d,\"edid\":null}\n",
                output_num, name, crtc_r->x, crtc_r->y, crtc_r->width, crtc_r->height);
        }
	free(crtc_r);
        free(name);
	free(out_r);
    }
    free(scr_r);
    xcb_disconnect(conn);
    return EXIT_SUCCESS;
}
