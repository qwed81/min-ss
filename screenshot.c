#include <X11/Xlib.h>
#include <X11/Xcursor/Xcursor.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdbool.h>
#include <png.h>

XGCValues gc_vals;

int min(int a, int b) {
	return (a < b) ? a : b;
}

int max(int a, int b) {
	return (a > b) ? a : b;
}

typedef struct Rect {
	int left;
	int top;
	int width;
	int height;
} Rect;

void calc_rect(Rect* rect, int x1, int y1, int x2, int y2) {
	rect->left = min(x1, x2);
	rect->top = min(y1, y2);

	int right = max(x1, x2);
	int bot = max(y1, y2);
	rect->width = right - rect->left;
	rect->height = bot - rect->top;
}

int write_png(FILE* file, XImage* full_ss, Rect* clip) {
	png_struct* png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL) {
		return -1;
	}

	png_info* info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
		return -1;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
		png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
		return -1;
	}

	// write the png to the file
	png_init_io(png_ptr, file);

	png_set_IHDR(png_ptr, info_ptr, clip->width, clip->height,
			8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	// set the title of the png
	png_text title_text;
	title_text.compression = PNG_TEXT_COMPRESSION_NONE;
	title_text.key = "Title";
	title_text.text = "Screenshot";
	png_set_text(png_ptr, info_ptr, &title_text, 1);
	png_write_info(png_ptr, info_ptr);

	// allocate enough space to fit one row of the clipped ss in png format
	png_byte* row = (png_byte*) malloc(3 * clip->width * sizeof(png_byte));

	// copy the screenshot over into png format
	for (int y = 0; y < clip->height; y += 1) {
		for (int x = 0; x < clip->width; x += 1) {
			int left = clip->left + x;
			int top = clip->top + y;

			// get the location of this pixel in the full ss
			char* data = (char*)(&full_ss->data[full_ss->width * top * 4 + left * 4]);
			row[x * 3] = data[2];
			row[x * 3 + 1] = data[1];
			row[x * 3 + 2] = data[0];
		}

		png_write_row(png_ptr, row);
	}

	png_write_end(png_ptr, NULL);

	// release the resouces used by libpng
	png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
	png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
	free(row);

	return 0;
}

int clip_ss(Display* d, int s, Window root, XWindowAttributes* root_attr, XImage* full_ss) {
	XSetWindowAttributes attrs = { 0 };
	attrs.override_redirect = 1;

	Window w = XCreateWindow(
		d, root,
		0, 0, root_attr->width, root_attr->height, 0,
		CopyFromParent, InputOutput, 
		CopyFromParent,
		CWOverrideRedirect | CWColormap | CWBackPixel | CWBorderPixel,
		&attrs
	);

	XSelectInput(d, w, ExposureMask | KeyPressMask | PointerMotionMask | ButtonPressMask | ButtonReleaseMask | FocusChangeMask);
	XMapWindow(d, w);

	gc_vals.function = GXcopy; 
	gc_vals.plane_mask = AllPlanes; 
	gc_vals.background = BlackPixel(d, s); 
	gc_vals.foreground = WhitePixel(d, s); 
	GC graphics = XCreateGC(d, w, GCFunction | GCPlaneMask | GCForeground | GCBackground, &gc_vals);

	Cursor c = XcursorLibraryLoadCursor(d, "tcross");
	XDefineCursor(d, w, c);

	int start_x;
	int start_y;
	int current_x;
	int current_y;
	Rect old_rect;
	int button_down = false;

	XEvent e;
	while (true) {
		XNextEvent(d, &e);
		if (e.type == FocusOut) {
			XGrabKeyboard(d, w, 0, GrabModeAsync, GrabModeAsync, CurrentTime);
		} else if (e.type == KeyPress) { // close on key press
			XDestroyWindow(d, w);
			return 1;
		} else if (e.type == Expose) { // redraw the whole screen
			XPutImage(d, w, graphics, full_ss, 0, 0, 0, 0, root_attr->width, root_attr->height);
			if (button_down) {
				Rect r;
				calc_rect(&r, start_x, start_y, current_x, current_y);
				XDrawRectangle(d, w, graphics, r.left, r.top, r.width, r.height);
			}
		} else if (e.type == MotionNotify && button_down == true) { // when the selection started, redraw the rectangle
			XMotionEvent* ev = (XMotionEvent*)(&e);
			int old_x = current_x;
			int old_y = current_y;

			current_x = ev->x_root;
			current_y = ev->y_root;

			// don't let the mouse go out of bounds
			if (current_x <= 0) {
				current_x = 1;
			}
			if (current_y <= 0) {
				current_y = 1;
			}

			// the sections that are dirty
			Rect rx;
			Rect ry;

			// depending if the rectangle is increasing or decreasing, change
			// what section of the image is redraw over it
			if (current_x > start_x && current_x < old_x ||
				current_x < start_x && current_x > old_x) { // x is decreasing

				calc_rect(&rx, current_x, start_y, old_x, old_y);
			} else { // x is increasing
				calc_rect(&rx, old_x, start_y, current_x, current_y);
			}

			if (current_y > start_y && current_y < old_y ||
				current_y < start_y && current_y > start_y) { // y is decreasing

				calc_rect(&ry, start_x, current_y, current_x, old_y);
			} else {
				calc_rect(&ry, start_x, old_y, old_x, current_y);
			}

			XPutImage(d, w, graphics, full_ss, rx.left - 1, rx.top - 1, rx.left - 1,
					rx.top - 1, rx.width + 2, rx.height + 2);

			XPutImage(d, w, graphics, full_ss, ry.left - 1, ry.top - 1, ry.left - 1,
					ry.top - 1, ry.width + 2, ry.height + 2);

			// if they are not both on the same side of the start
			// (the rectangle flipped sides), then we need to overwrite
			// the full thing
			if ((current_x - start_x) * (old_x - start_x) < 0 ||
				(current_y - start_y) * (old_y - start_y) < 0) {

				XPutImage(d, w, graphics, full_ss, 0, 0, 0,
						0, root_attr->width, root_attr->height);
			}

			Rect r;
			calc_rect(&r, start_x, start_y, current_x, current_y);
			XDrawRectangle(d, w, graphics, r.left, r.top, r.width, r.height);

			old_rect = r;
		} else if (e.type == ButtonPress) { // start selection on button press
			XButtonEvent* ev = (XButtonEvent*)(&e);
			// upon pressing not the left click, stop screen shotting
			if (ev->button != 1) {
				return 1;
			}

			start_x = ev->x;
			start_y = ev->y;
			if (start_x <= 0) {
				start_x = 1;
			}
			if (start_y <= 0) {
				start_y = 1;
			}

			current_x = start_x;
			current_y = start_y;

			button_down = true;
		} else if (e.type == ButtonRelease) { // finalize rectangle on button release
			if (button_down == true) {
				// prevent lag by destroying the window before finishing
				XDestroyWindow(d, w);
				XFlush(d);

				return write_png(stdout, full_ss, &old_rect);
			}
		} 
	}

	return 0;
}

bool should_clip(int argc, char** argv) {
	for(int i = 0; i < argc; i += 1) {
		if (strcmp(argv[i], "--full") == 0) {
			return false;
		}
	}

	return true;
}

int main(int argc, char** argv) {
	Display* d = XOpenDisplay(NULL);
	int s = DefaultScreen(d);
	Window root = RootWindow(d, s);

	if (d == NULL) {
		fprintf(stderr, "Can't open display\n");
		exit(1);
	}

	XWindowAttributes root_attr;
	XGetWindowAttributes(d, root, &root_attr);

	// we don't need to free the image because it lasts for the entire lifetime of the program
	XImage* image = XGetImage(d, root, 0, 0, root_attr.width, root_attr.height, AllPlanes, ZPixmap);
	if (image == NULL) {
		fprintf(stderr, "Can't get image\n");
		exit(1);
	}

	if (should_clip(argc, argv)) {
		if (clip_ss(d, s, root, &root_attr, image) != 0) {
			fprintf(stderr, "Could not write image\n");
			exit(1);
		}
	} else {
		Rect r;
		calc_rect(&r, 0, 0, image->width, image->height);
		if (write_png(stdout, image, &r) != 0) {
			fprintf(stderr, "Could not write image\n");
			exit(1);
		}
	}

	XCloseDisplay(d);
}
