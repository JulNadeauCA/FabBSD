/*	$FabBSD$	*/

#define CNC_CAPTURE_BUFSIZE 8192

/* Motion capture data frame */
struct cnc_capture_frame {
	cnc_real_t t;			/* Time (secs) */
	u_int32_t step;			/* Advance bits */
	u_int32_t dfl;			/* Direction bits */
};

#define CNC_CAPTURE_FRAME_INITIALIZER(t) { t, 0, 0 }

#ifdef MOTIONCAPTURE

#define CNC_CAPTURE_STEP(cf,bit,dir)					\
	if (cnc_capture) {						\
		(cf)->step |= (0x01 << (bit));				\
		if (dir)						\
			(cf)->dfl |= (0x01 << (bit));			\
	}
#define CNC_CAPTURE_FRAME(cf)						\
	if (cnc_capture) {						\
		if (cnc_capbuf_size < CNC_CAPTURE_BUFSIZE) {		\
			bcopy(cf, &cnc_capbuf[cnc_capbuf_size++],	\
			    sizeof(struct cnc_capture_frame));		\
		} else {						\
			printf("cnc: motioncap overflow\n");		\
		}							\
		wakeup(cnc_capbuf);					\
		tsleep(cnc_capbuf, PWAIT|PCATCH, "motcaprd", 0);	\
	}

extern struct cnc_capture_frame cnc_capbuf[CNC_CAPTURE_BUFSIZE];
extern int                      cnc_capbuf_size;

void cnc_capture_open(void);
void cnc_capture_close(void);
int cnc_capture_read(dev_t, struct uio *, int);

#else /* !MOTIONCAPTURE */

#define CNC_CAPTURE_STEP(cf,bit,dir)
#define CNC_CAPTURE_FRAME(cf)

#endif /* MOTIONCAPTURE */

extern int cnc_capture;
