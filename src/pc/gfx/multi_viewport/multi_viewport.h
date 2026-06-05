#ifndef MULTI_VIEWPORT_H
#define MULTI_VIEWPORT_H

// Data that is generally specified per-viewport

enum ViewportClearBuffer
{
    VIEW_CLEAR_BUFFER_NONE  = 0,      // 00: The viewport will not have a buffer cleared.
	VIEW_CLEAR_BUFFER_COLOR = 0b01,   // 01: The viewport's color buffer will be cleared
	VIEW_CLEAR_BUFFER_DEPTH = 0b10,   // 10: The viewport's depth buffer will be cleared
	VIEW_CLEAR_BUFFER_BOTH  = 0b11    // 11: The viewport's color and depth buffers will be cleared
};

#endif