#ifndef _PUBLIC_H
#define _PUBLIC_H

#define IOCTL_LEFT            0x810
#define IOCTL_RIGHT			0x819
#define IOCTL_ABS            0x801
#define IOCTL_REL			0x802
#define IOCTL_X_REL            0x813
#define IOCTL_Y_REL            0x814
#define IOCTL_HOLD_LEFT            0x815
#define IOCTL_HOLD_RIGHT            0x816
#define IOCTL_RELEASE_LEFT            0x817
#define IOCTL_RELEASE_RIGHT            0x818
#define IOCTL_SCROLLUP            0x803
#define IOCTL_SCROLLDOWN            0x804

#define IOCTL_MOUFILTR_LEFT_CLICK CTL_CODE(	FILE_DEVICE_MOUSE,   \
                                        IOCTL_LEFT,    \
										METHOD_IN_DIRECT,    \
										FILE_WRITE_DATA | FILE_READ_DATA)

#define IOCTL_MOUFILTR_RIGHT_CLICK CTL_CODE(	FILE_DEVICE_MOUSE,   \
                                        IOCTL_RIGHT,    \
										METHOD_IN_DIRECT,    \
										FILE_WRITE_DATA | FILE_READ_DATA)

#define IOCTL_MOUFILTR_MOVE_ABSOLUTE CTL_CODE(	FILE_DEVICE_MOUSE,   \
                                        IOCTL_ABS,    \
										METHOD_IN_DIRECT,    \
										FILE_WRITE_DATA | FILE_READ_DATA)

#define IOCTL_MOUFILTR_MOVE_RELATIVE CTL_CODE(	FILE_DEVICE_MOUSE,   \
                                        IOCTL_REL,    \
										METHOD_IN_DIRECT,    \
										FILE_WRITE_DATA | FILE_READ_DATA)

#define IOCTL_MOUFILTR_MOVE_X_RELATIVE CTL_CODE(	FILE_DEVICE_MOUSE,   \
                                        IOCTL_X_REL,    \
										METHOD_IN_DIRECT,    \
										FILE_WRITE_DATA | FILE_READ_DATA)

#define IOCTL_MOUFILTR_MOVE_Y_RELATIVE CTL_CODE(	FILE_DEVICE_MOUSE,   \
                                        IOCTL_Y_REL,    \
										METHOD_IN_DIRECT,    \
										FILE_WRITE_DATA | FILE_READ_DATA)

#define IOCTL_MOUFILTR_HOLD_LEFT CTL_CODE(	FILE_DEVICE_MOUSE,   \
                                        IOCTL_HOLD_LEFT,    \
										METHOD_IN_DIRECT,    \
										FILE_WRITE_DATA | FILE_READ_DATA)

#define IOCTL_MOUFILTR_HOLD_RIGHT CTL_CODE(	FILE_DEVICE_MOUSE,   \
                                        IOCTL_HOLD_RIGHT,    \
										METHOD_IN_DIRECT,    \
										FILE_WRITE_DATA | FILE_READ_DATA)

#define IOCTL_MOUFILTR_RELEASE_LEFT CTL_CODE(	FILE_DEVICE_MOUSE,   \
                                        IOCTL_RELEASE_LEFT,    \
										METHOD_IN_DIRECT,    \
										FILE_WRITE_DATA | FILE_READ_DATA)

#define IOCTL_MOUFILTR_RELEASE_RIGHT CTL_CODE(	FILE_DEVICE_MOUSE,   \
                                        IOCTL_RELEASE_RIGHT,    \
										METHOD_IN_DIRECT,    \
										FILE_WRITE_DATA | FILE_READ_DATA)
#define IOCTL_MOUFILTR_SCROLLUP CTL_CODE(	FILE_DEVICE_MOUSE,   \
										IOCTL_SCROLLUP, \
										METHOD_IN_DIRECT, \
										FILE_WRITE_DATA | FILE_READ_DATA)

#define IOCTL_MOUFILTR_SCROLLDOWN CTL_CODE(	FILE_DEVICE_MOUSE,   \
										IOCTL_SCROLLDOWN, \
										METHOD_IN_DIRECT, \
										FILE_WRITE_DATA | FILE_READ_DATA)



#endif
