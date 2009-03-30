/*	$FabBSD$	*/
/*	Public domain	*/

/*
 * An example FabBSD program for controlling the manipulator of a typical
 * parts cleaning station, consisting of 3 motorized axes (servo(4) devices)
 * and one sensing gripper(4) device. If a no parts are available at the
 * specified pickup location, pickup is retried in a loop.
 */

#include <stdlib.h>
#include <unistd.h>
#include <cnc.h>

/* Width of the part */
const cnc_pos_t partWidthMin = 1/16;
const cnc_pos_t partWidthMax = 1/8;

/* Pickup home position */
const cnc_vec_t pickPos = { 0.0, 10.0, 10.0 };

/* Dipping time in seconds */
#define NTANKS 4
int dipSecs[NTANKS] = { 30, 60, 30, 10 };

/* Dipping depth relative to Z home */
cnc_pos_t dipDepth[NTANKS] = { 3.0, 3.0, 3.0, 3.0 };

/* Tank center coordinates */
cnc_pos_t xTank[NTANKS] = { 5.0, 15.0, 25.0, 35.0 };
cnc_pos_t yTank[NTANKS] = { 5.0, 5.0, 5.0, 5.0 };

int
main(int argc, char *argv[])
{
	int ch, i, nPart = 0;
	
	while ((ch = getopt(argc, argv, "w:p:?h")) != -1) {
		switch (ch) {
		case 'w':
			if (CNC_ParseRange(optarg, &partWidthMin,
			    &partWidthMax) == -1)
				goto fail;
			break;
		case 'p':
			if (CNC_ParsePosition(optarg, &pickPos) == -1)
				goto fail;
			break;
		default:
			printf("Usage: %s [-w min-max] [-p pickpos]\n",
			    __progname);
			return (1);
		}
	}

	/* Move arm to home position */
	movehome();

	while (!estop()) {
pick:
		/* Pick the part */
		fastmove(pickPos.v);
		gripctl(0, GRIPPER_OPEN);
		fastmoveY(partWidthMax + 1/16);
		gripctl(0, GRIPPER_CLOSE);
		if (gripwidth(0) < partWidthMin) {
			sleep(1);
			beep();
			printf("Gripper failed, retrying\n");
			goto pick;
		}
		
		fastmoveY(0.0);

		for (i = 0; i < NTANKS; i++) {
			/* Fast move to center of tank. */
			fastmove_inc(xTank[i], yTank[i], 0.0);
			/* Slowly dip to specified depth. */
			slowmoveZ_inc(dipDepth[i]);
			sleep(dipSecs(i));
			slowmoveZ(0.0, 10000.0);
		}

		/*
		 * At this point we could, e.g., send a message to another
		 * manipulator involved in the assembly.
		 */
		printf("Cleaning of part %d completed\n", nPart++);
	}
	return (0);
}
