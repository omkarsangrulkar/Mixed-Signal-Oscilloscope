/*
 * z_touch_XPT2046_test.c
 *
 *  Created on: 2 giu 2022
 *      Author: mauro
 *
 *  This is related to the functions testing features and showing performance
 *  you don't need this file in the production project
 *
 *  licensing: https://github.com/maudeve-it/ILI9XXX-XPT2046-STM32/blob/c097f0e7d569845c1cf98e8d930f2224e427fd54/LICENSE
 *
 *  Do you want to test functions?
 *  follow STEP_1 in z_displ_ILI9488_test.c
 *  then:
 *
 *  STEP_2
 *	in the main loop remove or comment previous command and put:
 *	Touch_ShowData();					// shows data returned by XPT2046 controller: try touching everywhere the display
 *
 *  STEP_3
 *	in the main loop remove or comment previous command and put:
 *	Touch_TestCalibration();			// compute and shows parameters to adopt converting XPT2046 data into display coordinates.
 *		 								// compare the (green) data shown with data in z_touch_XPT2046.h and, in case of needs, change it.
 *
 *  STEP_4
 *	in the main loop remove or comment previous command and put:
 *	Touch_TestDrawing();				//move pen on display while touching to check quality of parameters detected on step 3
 *										// repeat test on 4 display orientation
 *
 */


#include "main.h"

extern int16_t _width;       								///< (oriented) display width
extern int16_t _height;      								///< (oriented) display height




/*************************************************************
 * used by Touch_TestDrawing() and Touch_TestCalibration()
 *************************************************************/
void DrawCross(uint16_t x,uint16_t y,uint16_t fcol){
	uint8_t ray=10;
//	Displ_Line(x-ray, y-ray, x+ray, y+ray, fcol);
//	Displ_Line(x-ray, y+ray, x+ray, y-ray, fcol);
	Displ_Line(x-ray, y, x+ray, y, fcol);
	Displ_Line(x, y-ray, x, y+ray, fcol);

}





/*************************************************************
 * used by Touch_TestDrawing() and Touch_TestCalibration()
 *************************************************************/
void MoveCross(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2,uint16_t fcol,uint16_t bcol){
	const uint8_t steps=20;
	int16_t deltax,deltay;

	deltax = (x2-x1)/steps;
	deltay = (y2-y1)/steps;

	while ((x1!=x2) || (y1!=y2)) {
		DrawCross(x1,y1,bcol);

		x1=((abs((x2-x1))>abs(deltax)) ? (x1+deltax) : x2);
		y1=((abs((y2-y1))>abs(deltay)) ? (y1+deltay) : y2);

		DrawCross(x1,y1,fcol);
		HAL_Delay(500/steps);
	}
}









extern uint16_t Touch_PollAxis(uint8_t axis);

extern volatile uint8_t Touch_PenDown;
void Test_TouchInterrupt(void) {
    char text[60];
    uint16_t test_duration = 100;  // Run for 10 seconds

    Displ_Orientation(Displ_Orientat_270);
    Displ_CLS(WHITE);

    // Title
    Displ_WString(10, 10, "TOUCH INTERRUPT TEST", Font20, 1, BLUE, WHITE);
    Displ_WString(10, 35, "Touch the screen!", Font16, 1, RED, WHITE);

    // Instructions
    Displ_WString(10, 60, "INT Pin: Physical pin state", Font12, 1, BLACK, WHITE);
    Displ_WString(10, 75, "PenDown: Interrupt flag", Font12, 1, BLACK, WHITE);

    for(uint16_t i = 0; i < test_duration; i++) {
        // Read the physical pin state
        uint8_t pin_state = HAL_GPIO_ReadPin(TOUCH_INT_GPIO_Port, TOUCH_INT_Pin);
        sprintf(text, "INT Pin: %d  %s    ",
                pin_state,
                pin_state ? "(Not touching)" : "(TOUCHING!)   ");
        Displ_WString(10, 100, text, Font16, 1,
                     pin_state ? BLACK : RED, WHITE);

        // Check the interrupt flag
        sprintf(text, "PenDown: %d  %s    ",
                Touch_PenDown,
                Touch_PenDown ? "(Interrupt fired!)" : "(No interrupt)    ");
        Displ_WString(10, 120, text, Font16, 1,
                     Touch_PenDown ? GREEN : BLACK, WHITE);

        // Counter
        sprintf(text, "Time: %d / %d sec    ", i/10, test_duration/10);
        Displ_WString(10, 145, text, Font12, 1, BLUE, WHITE);

        HAL_Delay(100);
    }

    // Final summary
    Displ_FillArea(0, 170, 480, 100, DD_YELLOW);
    if (Touch_PenDown) {
        Displ_WString(10, 180, "SUCCESS!", Font20, 1, GREEN, DD_YELLOW);
        Displ_WString(10, 205, "Touch interrupt is working", Font16, 1, BLACK, DD_YELLOW);
    } else {
        Displ_WString(10, 180, "FAILED!", Font20, 1, RED, DD_YELLOW);
        Displ_WString(10, 205, "Check INT pin connection", Font16, 1, BLACK, DD_YELLOW);
    }
    HAL_Delay(3000);
}

void Test_TouchSPI(void) {
    char text[60];
    uint16_t x_raw, y_raw, z_raw;
    uint16_t test_duration = 100;  // Run for 10 seconds

    Displ_Orientation(Displ_Orientat_270);
    Displ_CLS(WHITE);

    // Title and instructions
    Displ_WString(10, 10, "TOUCH SPI TEST", Font20, 1, BLUE, WHITE);
    Displ_WString(10, 35, "Touch screen NOW!", Font16, 1, RED, WHITE);

    // Threshold info
    sprintf(text, "X Threshold: 0x%04X", X_THRESHOLD);
    Displ_WString(10, 60, text, Font12, 1, BLACK, WHITE);
    sprintf(text, "Z Threshold: 0x%04X", Z_THRESHOLD);
    Displ_WString(10, 75, text, Font12, 1, BLACK, WHITE);

    for(uint16_t i = 0; i < test_duration; i++) {
        // Poll each axis directly
        z_raw = Touch_PollAxis(Z_AXIS);
        x_raw = Touch_PollAxis(X_AXIS);
        y_raw = Touch_PollAxis(Y_AXIS);

        // Display raw values
        sprintf(text, "X: 0x%04X (%5d)    ", x_raw, x_raw);
        Displ_WString(10, 100, text, Font16, 1,
                     (x_raw > X_THRESHOLD) ? GREEN : BLACK, WHITE);

        sprintf(text, "Y: 0x%04X (%5d)    ", y_raw, y_raw);
        Displ_WString(10, 120, text, Font16, 1,
                     (y_raw > 0x0500) ? GREEN : BLACK, WHITE);

        sprintf(text, "Z: 0x%04X (%5d)    ", z_raw, z_raw);
        Displ_WString(10, 140, text, Font16, 1,
                     (z_raw > Z_THRESHOLD) ? GREEN : BLACK, WHITE);

        // Touch detection status
        uint8_t touch_detected = (z_raw > Z_THRESHOLD) && (x_raw > X_THRESHOLD);
        Displ_FillArea(10, 170, 300, 30, touch_detected ? GREEN : RED);
        sprintf(text, "TOUCH: %s    ", touch_detected ? "DETECTED" : "NO      ");
        Displ_WString(10, 175, text, Font20, 1, WHITE,
                     touch_detected ? GREEN : RED);

        // Counter
        sprintf(text, "Time: %d / %d sec    ", i/10, test_duration/10);
        Displ_WString(10, 210, text, Font12, 1, BLUE, WHITE);

        HAL_Delay(100);
    }

    HAL_Delay(2000);
}


void Touch_ShowData_Fixed(void)
{
    uint16_t x_touch;
    uint16_t y_touch;
    uint16_t z_touch;
    char text[40];
    uint32_t touchTime = 0;
    uint8_t wasTouching = 0;
    uint32_t loopCount = 0;

    Displ_Orientation(Displ_Orientat_270);
    Displ_FillArea(0, 0, _width, _height, WHITE);

    // Title
    Displ_WString(10, 5, "TOUCH TEST - CONTINUOUS", Font16, 1, BLUE, WHITE);
    Displ_WString(10, 25, "Touch and hold screen", Font12, 1, RED, WHITE);

    // Draw threshold lines for reference
    Displ_Line(0, 45, _width, 45, DD_BLUE);

    while (1) {
        loopCount++;

        // CONTINUOUSLY poll the touch controller (don't wait for interrupt)
        z_touch = Touch_PollAxis(Z_AXIS);
        x_touch = Touch_PollAxis(X_AXIS);
        y_touch = Touch_PollAxis(Y_AXIS);

        // Determine if currently touching based on thresholds
        uint8_t isTouching = (z_touch > Z_THRESHOLD) && (x_touch > X_THRESHOLD);

        if (isTouching) {
            if (!wasTouching) {
                // Touch just started
                touchTime = HAL_GetTick();
                wasTouching = 1;
            }

            // Display PENDOWN status (large and visible)
            Displ_FillArea(10, 50, 200, 25, RED);
            strcpy(text, " PENDOWN ");
            Displ_WString(20, 53, text, Font20, 1, WHITE, RED);

            // Display raw hex values
            sprintf(text, "X = 0x%04X  (%5d)    ", x_touch, x_touch);
            Displ_WString(10, 85, text, Font16, 1, BLUE, WHITE);

            sprintf(text, "Y = 0x%04X  (%5d)    ", y_touch, y_touch);
            Displ_WString(10, 105, text, Font16, 1, BLUE, WHITE);

            sprintf(text, "Z = 0x%04X  (%5d)    ", z_touch, z_touch);
            Displ_WString(10, 125, text, Font16, 1,
                         (z_touch > Z_THRESHOLD*2) ? GREEN : BLUE, WHITE);

            // Display touch duration
            uint32_t touchDuration = HAL_GetTick() - touchTime;
            sprintf(text, "Hold time: %lu ms     ", touchDuration);
            Displ_WString(10, 150, text, Font12, 1, GREEN, WHITE);

            // Show thresholds being exceeded
            sprintf(text, "X > Threshold: %s   ",
                   (x_touch > X_THRESHOLD) ? "YES" : "NO ");
            Displ_WString(10, 170, text, Font12, 1,
                         (x_touch > X_THRESHOLD) ? GREEN : RED, WHITE);

            sprintf(text, "Z > Threshold: %s   ",
                   (z_touch > Z_THRESHOLD) ? "YES" : "NO ");
            Displ_WString(10, 185, text, Font12, 1,
                         (z_touch > Z_THRESHOLD) ? GREEN : RED, WHITE);

        } else {
            if (wasTouching) {
                // Touch just released
                wasTouching = 0;
            }

            // Clear PENDOWN status
            Displ_FillArea(10, 50, 200, 25, WHITE);
            strcpy(text, "           ");
            Displ_WString(20, 53, text, Font20, 1, BLACK, WHITE);

            // Still show values even when not touching (dimmed)
            sprintf(text, "X = 0x%04X  (%5d)    ", x_touch, x_touch);
            Displ_WString(10, 85, text, Font12, 1, D_BLUE, WHITE);

            sprintf(text, "Y = 0x%04X  (%5d)    ", y_touch, y_touch);
            Displ_WString(10, 105, text, Font12, 1, D_BLUE, WHITE);

            sprintf(text, "Z = 0x%04X  (%5d)    ", z_touch, z_touch);
            Displ_WString(10, 125, text, Font12, 1, D_BLUE, WHITE);

            sprintf(text, "Waiting for touch...     ");
            Displ_WString(10, 150, text, Font12, 1, BLACK, WHITE);

            // Show why not detecting
            if (z_touch <= Z_THRESHOLD) {
                sprintf(text, "Z too low (<%#X)    ", Z_THRESHOLD);
                Displ_WString(10, 170, text, Font12, 1, RED, WHITE);
            }
            if (x_touch <= X_THRESHOLD) {
                sprintf(text, "X too low (<%#X)    ", X_THRESHOLD);
                Displ_WString(10, 185, text, Font12, 1, RED, WHITE);
            }
        }

        // Loop counter to show it's running
        sprintf(text, "Loop: %lu     ", loopCount);
        Displ_WString(10, 210, text, Font12, 1, MAGENTA, WHITE);

        // Polling rate
        sprintf(text, "Uptime: %lu sec     ", HAL_GetTick() / 1000);
        Displ_WString(10, 225, text, Font12, 1, BLACK, WHITE);

        HAL_Delay(50);  // Poll at ~20Hz (50ms interval)
    }
}

/*****************************************************
 * polls the 3 axis of touch device and shows values returned
 *****************************************************/
void Touch_ShowData(void)
{
	uint16_t x_touch;
	uint16_t y_touch;
	uint16_t z_touch;
	char text[30];
	uint32_t touchTime=0,touchDelay;

	Displ_Orientation(Displ_Orientat_270);

	Displ_FillArea(0,0,_width,_height,WHITE);

	while (1) {

		if (Touch_GotATouch(1))
			touchTime=HAL_GetTick();
		touchDelay=(HAL_GetTick() - touchTime);

		z_touch = Touch_PollAxis(Z_AXIS);
		x_touch = Touch_PollAxis(X_AXIS);
		y_touch = Touch_PollAxis(Y_AXIS);

		if ((touchDelay<100) && (touchTime!=0)) {
			strcpy(text,"PENDOWN");
			Displ_WString(10,30,text,Font20,1,RED,YELLOW);
		};
		if (touchDelay>=100) {
			strcpy(text,"       ");
			Displ_WString(10,30,text,Font20,1,BLUE,WHITE);
			touchDelay=0;
		};


		sprintf(text,"X=%#X -         ",x_touch);
		Displ_WString(10,60,text,Font20,1,BLUE,WHITE);
		sprintf(text,"Y=%#X -         ",y_touch);
		Displ_WString(10,80,text,Font20,1,BLUE,WHITE);
		sprintf(text,"Z=%#X -         ",z_touch);
		Displ_WString(10,100,text,Font20,1,BLUE,WHITE);
		HAL_Delay(100);
	}

}




/****************************************
 * a test with a continue touch polling,
 * drawing values returned,
 * until touch is released
 ****************************************/
void Touch_TestDrawing() {
	uint16_t px=0,py,npx,npy;
	uint8_t isTouch;

	for (uint8_t k=0;k<4;k++){

		switch (k){
		case 0:
			Displ_Orientation(Displ_Orientat_0);
			break;
		case 1:
			Displ_Orientation(Displ_Orientat_90);
			break;
		case 2:
			Displ_Orientation(Displ_Orientat_180);
			break;
		case 3:
			Displ_Orientation(Displ_Orientat_270);
			break;
		}

		Displ_CLS(DD_BLUE);
		Displ_CString(0,10,_width,Font12.Height+10,"Touch and drag over display",Font12,1,WHITE,DD_BLUE);

		Touch_GotATouch(1);
		Touch_WaitForTouch(0);

		while (1) {
			Touch_GetXYtouch(&npx,&npy,&isTouch);
			if (!isTouch) //if there is no touch: stop drawing
				break;
			if (px!=0)
				DrawCross(px,py,DD_BLUE);
			DrawCross(npx,npy,WHITE);
			px=npx;
			py=npy;
			HAL_Delay(30);
		}
	}
}






void Touch_TestCalibration(){
	uint8_t correct_orientation=0;
	Displ_Orientat_e orientation=Displ_Orientat_0;

#ifdef ILI9488
	const uint16_t shift=80;
#endif
#ifdef ILI9341
	const uint16_t shift=50;
#endif

	char text[30];
	uint16_t x[5];
	uint16_t y[5];
	uint32_t read_x[5]={0,0,0,0,0};
	uint32_t read_y[5]={0,0,0,0,0};
	uint32_t r_x,r_y,r_z;
	uint32_t xx,yy;
	uint8_t k,h;
	float ax[2];
	float bx[2];
	float ay[2];
	float by[2];
	float axx,bxx,ayy,byy,e;
//	uint8_t orientation;
	sFONT font;

	while (! correct_orientation) {
		Displ_CLS(WHITE);
		Displ_Orientation(orientation);
		//setting positions for calibration
		x[0]=shift;
		x[1]=_width-shift;
		x[2]=shift;
		x[3]=_width-shift;
		x[4]=_width>>1;
		y[0]=shift;
		y[1]=_height-shift;
		y[2]=_height-shift;
		y[3]=shift;
		y[4]=_height>>1;
		for (uint8_t k=0; k<2; k++) {
			switch (k) {
			case 0:
				strcpy(text,"Press and briefly hold");
				break;
			case 1:
				strcpy(text,"stylus on the target.");
				break;
			}
			Displ_CString(0,10+Font12.Height*k,_width,10+Font12.Height*(1+k),text,Font12,1,BLACK,WHITE);

		}
		for (uint8_t k=0; k<2; k++) {
			switch (k) {
			case 0:
				strcpy(text,"Repeat as the target");
				break;
			case 1:
				strcpy(text,"moves around the screen.");
				break;
			}
			Displ_CString(0,_height+Font12.Height*(k-2)-10,_width,_height+Font12.Height*(k-1)-10,text,Font12,1,BLACK,WHITE);

		}

		HAL_Delay(1000);
		Touch_WaitForUntouch(0);

		for (h=0;h<5;h++){    // 5 point calibration
			DrawCross(x[h],y[h],BLACK);
	// wait for stylus
			Touch_WaitForTouch(0);

	// makes NUM_READINGS touch polling calculating average value
			k=0;
			while (k<NUM_READINGS) {
				r_x=Touch_PollAxis(X_AXIS);
				r_y=Touch_PollAxis(Y_AXIS);
				r_z=Touch_PollAxis(Z_AXIS);
				if ((r_z>Z_THRESHOLD) && (r_x>X_THRESHOLD)) {
					read_x[h]+=r_x;
					read_y[h]+=r_y;
					k++;
					HAL_Delay(10);
				}

			}
			read_x[h]=read_x[h]/NUM_READINGS;
			read_y[h]=read_y[h]/NUM_READINGS;

			if (h!=4)
				MoveCross(x[h],y[h],x[h+1],y[h+1],BLACK,WHITE);

			// wait for user removing stylus
			Touch_WaitForUntouch(0);
		}


		//check il display and touch_sensor orientation are aligned

		correct_orientation=1;
		correct_orientation &= (read_x[1]>read_x[0]);
		correct_orientation &= (read_y[1]>read_y[0]);
		correct_orientation &= (read_x[2]<read_x[1]);
		correct_orientation &= (read_x[3]>read_x[2]);
		correct_orientation &= (read_y[3]<read_y[2]);
		correct_orientation &= (read_x[4]<read_x[3]);
		correct_orientation &= (read_y[4]>read_y[3]);

		if (! correct_orientation){   //they could be alighen but inverted x axes
			correct_orientation=1;
			correct_orientation &= (read_x[1]<read_x[0]);
			correct_orientation &= (read_y[1]>read_y[0]);
			correct_orientation &= (read_x[2]>read_x[1]);
			correct_orientation &= (read_x[3]<read_x[2]);
			correct_orientation &= (read_y[3]<read_y[2]);
			correct_orientation &= (read_x[4]>read_x[3]);
			correct_orientation &= (read_y[4]>read_y[3]);
		}


		if (! correct_orientation){  // if not aligned, rotate display
			Displ_CLS(WHITE);
			Displ_CString(0,((_height>>1)-31),_width,((_height>>1)-10),"please",Font20,1,BLUE,WHITE);
			Displ_CString(0,((_height>>1)-11),_width,((_height>>1)+10),"repeat",Font20,1,BLUE,WHITE);
			Displ_CString(0,((_height>>1)+11),_width,((_height>>1)+20),"calibration",Font20,1,BLUE,WHITE);
			HAL_Delay(2000);
			switch (orientation) {
			case Displ_Orientat_0:
				orientation=Displ_Orientat_90;
				break;
			case Displ_Orientat_90:
				orientation=Displ_Orientat_180;
				break;
			case Displ_Orientat_180:
				orientation=Displ_Orientat_270;
				break;
			case Displ_Orientat_270:
				orientation=Displ_Orientat_0;
				break;

			}

		}

	}



	//calculate linear conversion parameter between point 1 and 2

	ax[0]=(x[0]+0.0f)-x[1];
	bx[0]=((x[1]+0.0f)*read_x[0])-((x[0]+0.0f)*read_x[1]);
	e=((read_x[0]+0.0f)-read_x[1]);
	ax[0]=ax[0]/e;
	bx[0]=bx[0]/e;

	ay[0]=(y[0]+0.0f)-y[1];
	by[0]=((y[1]+0.0f)*read_y[0])-((y[0]+0.0f)*read_y[1]);
	ay[0]=ay[0]/((read_y[0]+0.0f)-read_y[1]);
	by[0]=by[0]/((read_y[0]+0.0f)-read_y[1]);

	//calculate linear conversion parameter between point 3 and 4
	ax[1]=(x[2]+0.0f)-x[3];
	bx[1]=((x[3]+0.0f)*read_x[2])-((x[2]+0.0f)*read_x[3]);
	e=((read_x[2]+0.0f)-read_x[3]);
	ax[1]=ax[1]/e;
	bx[1]=bx[1]/e;

	ay[1]=(y[2]+0.0f)-y[3];
	by[1]=((y[3]+0.0f)*read_y[2])-((y[2]+0.0f)*read_y[3]);
	ay[1]=ay[1]/((read_y[2]+0.0f)-read_y[3]);
	by[1]=by[1]/((read_y[2]+0.0f)-read_y[3]);


	// calculate average conversion parameters

	axx = (ax[0] + ax[1])/2;
	bxx = (bx[0] + bx[1])/2;
	ayy = (ay[0] + ay[1])/2;
	byy = (by[0] + by[1])/2;


	Displ_CLS(WHITE);

#ifdef ILI9488
	font=Font16;
#endif
#ifdef ILI9341
	font=Font12;
#endif

	k=1;
	sprintf(text,"Current config:");
	Displ_WString(10,10+Font12.Height*k++,text,font,1,BLACK,WHITE);
	sprintf(text,"Ax=%f Bx=%f",AX,BX);
	Displ_WString(10,10+Font12.Height*k++,text,font,1,BLACK,WHITE);
	sprintf(text,"Ay=%f By=%f",AY,BY);
	Displ_WString(10,10+Font12.Height*k++,text,font,1,BLACK,WHITE);
#ifdef  T_ROTATION_0
	sprintf(text,"Orientation 0");
#endif
#ifdef  T_ROTATION_90
	sprintf(text,"Orientation 90");
#endif
#ifdef  T_ROTATION_180
	sprintf(text,"Orientation 180");
#endif
#ifdef  T_ROTATION_270
	sprintf(text,"Orientation 270");
#endif
	Displ_WString(10,10+Font12.Height*k++,text,font,1,BLACK,WHITE);
	k++;
	sprintf(text,"Current test:");
	Displ_WString(10,10+font.Height*k++,text,font,1,BLACK,WHITE);
	sprintf(text,"Ax=%f Bx=%f",ax[0],bx[0]);
	Displ_WString(10,10+font.Height*k++,text,font,1,RED,WHITE);
	sprintf(text,"Ay=%f By=%f",ay[0],by[0]);
	Displ_WString(10,10+font.Height*k++,text,font,1,RED,WHITE);
	sprintf(text,"Ax=%f Bx=%f",ax[1],bx[1]);
	Displ_WString(10,10+font.Height*k++,text,font,1,BLUE,WHITE);
	sprintf(text,"Ay=%f By=%f",ay[1],by[1]);
	Displ_WString(10,10+font.Height*k++,text,font,1,BLUE,WHITE);
	k++;
	sprintf(text,"Proposed config:");
	Displ_WString(10,10+font.Height*k++,text,font,1,BLACK,WHITE);
	sprintf(text,"Ax=%f Bx=%f",axx,bxx);
	Displ_WString(10,10+font.Height*k++,text,font,1,DD_GREEN,WHITE);
	sprintf(text,"Ay=%f By=%f",ayy,byy);
	Displ_WString(10,10+font.Height*k++,text,font,1,DD_GREEN,WHITE);
	switch (orientation) {
	case 0:
		sprintf(text,"Orientation 0");
		break;
	case 1:
		sprintf(text,"Orientation 270");
		break;
	case 2:
		sprintf(text,"Orientation 180");
		break;
	case 3:
		sprintf(text,"Orientation 90");
	}
	Displ_WString(10,10+font.Height*k++,text,font,1,DD_GREEN,WHITE);

	xx=(ax[0]*read_x[4]+bx[0]);
	yy=(ay[0]*read_y[4]+by[0]);
	DrawCross(xx,yy,RED);
	xx=(ax[1]*read_x[4]+bx[1]);
	yy=(ay[1]*read_y[4]+by[1]);
	DrawCross(xx,yy,BLUE);
	xx=(axx*read_x[4]+bxx);
	yy=(ayy*read_y[4]+byy);
	DrawCross(xx,yy,DD_GREEN);

	Touch_WaitForTouch(0);
}

void Touch_TestCalibration_Landscape(){
    // FORCE landscape mode - don't auto-rotate!
    Displ_Orientat_e orientation = Displ_Orientat_90;

#ifdef ILI9488
    const uint16_t shift=80;
#endif
#ifdef ILI9341
    const uint16_t shift=50;
#endif

    char text[30];
    uint16_t x[5];
    uint16_t y[5];
    uint32_t read_x[5]={0,0,0,0,0};
    uint32_t read_y[5]={0,0,0,0,0};
    uint32_t r_x,r_y,r_z;
    uint32_t xx,yy;
    uint8_t k,h;
    float ax[2];
    float bx[2];
    float ay[2];
    float by[2];
    float axx,bxx,ayy,byy,e;
    sFONT font;

    // Single calibration pass - no rotation loop
    Displ_CLS(WHITE);
    Displ_Orientation(orientation);  // LANDSCAPE (90°)

    //setting positions for calibration
    x[0]=shift;
    x[1]=_width-shift;
    x[2]=shift;
    x[3]=_width-shift;
    x[4]=_width>>1;
    y[0]=shift;
    y[1]=_height-shift;
    y[2]=_height-shift;
    y[3]=shift;
    y[4]=_height>>1;

    for (uint8_t k=0; k<2; k++) {
        switch (k) {
        case 0:
            strcpy(text,"Press and briefly hold");
            break;
        case 1:
            strcpy(text,"stylus on the target.");
            break;
        }
        Displ_CString(0,10+Font12.Height*k,_width,10+Font12.Height*(1+k),text,Font12,1,BLACK,WHITE);
    }

    for (uint8_t k=0; k<2; k++) {
        switch (k) {
        case 0:
            strcpy(text,"Repeat as the target");
            break;
        case 1:
            strcpy(text,"moves around the screen.");
            break;
        }
        Displ_CString(0,_height+Font12.Height*(k-2)-10,_width,_height+Font12.Height*(k-1)-10,text,Font12,1,BLACK,WHITE);
    }

    HAL_Delay(1000);
    Touch_WaitForUntouch(0);

    // Collect 5 calibration points
    for (h=0;h<5;h++){
        DrawCross(x[h],y[h],BLACK);

        // wait for stylus
        Touch_WaitForTouch(0);

        // makes NUM_READINGS touch polling calculating average value
        k=0;
        while (k<NUM_READINGS) {
            r_x=Touch_PollAxis(X_AXIS);
            r_y=Touch_PollAxis(Y_AXIS);
            r_z=Touch_PollAxis(Z_AXIS);
            if ((r_z>Z_THRESHOLD) && (r_x>X_THRESHOLD)) {
                read_x[h]+=r_x;
                read_y[h]+=r_y;
                k++;
                HAL_Delay(10);
            }
        }
        read_x[h]=read_x[h]/NUM_READINGS;
        read_y[h]=read_y[h]/NUM_READINGS;

        if (h!=4)
            MoveCross(x[h],y[h],x[h+1],y[h+1],BLACK,WHITE);

        // wait for user removing stylus
        Touch_WaitForUntouch(0);
    }

    // Calculate linear conversion parameters between point 1 and 2
    ax[0]=(x[0]+0.0f)-x[1];
    bx[0]=((x[1]+0.0f)*read_x[0])-((x[0]+0.0f)*read_x[1]);
    e=((read_x[0]+0.0f)-read_x[1]);
    ax[0]=ax[0]/e;
    bx[0]=bx[0]/e;

    ay[0]=(y[0]+0.0f)-y[1];
    by[0]=((y[1]+0.0f)*read_y[0])-((y[0]+0.0f)*read_y[1]);
    ay[0]=ay[0]/((read_y[0]+0.0f)-read_y[1]);
    by[0]=by[0]/((read_y[0]+0.0f)-read_y[1]);

    // Calculate linear conversion parameters between point 3 and 4
    ax[1]=(x[2]+0.0f)-x[3];
    bx[1]=((x[3]+0.0f)*read_x[2])-((x[2]+0.0f)*read_x[3]);
    e=((read_x[2]+0.0f)-read_x[3]);
    ax[1]=ax[1]/e;
    bx[1]=bx[1]/e;

    ay[1]=(y[2]+0.0f)-y[3];
    by[1]=((y[3]+0.0f)*read_y[2])-((y[2]+0.0f)*read_y[3]);
    ay[1]=ay[1]/((read_y[2]+0.0f)-read_y[3]);
    by[1]=by[1]/((read_y[2]+0.0f)-read_y[3]);

    // Calculate average conversion parameters
    axx = (ax[0] + ax[1])/2;
    bxx = (bx[0] + bx[1])/2;
    ayy = (ay[0] + ay[1])/2;
    byy = (by[0] + by[1])/2;

    // Display results
    Displ_CLS(WHITE);

#ifdef ILI9488
    font=Font16;
#endif
#ifdef ILI9341
    font=Font12;
#endif

    k=1;
    sprintf(text,"Current config:");
    Displ_WString(10,10+Font12.Height*k++,text,font,1,BLACK,WHITE);
    sprintf(text,"Ax=%f Bx=%f",AX,BX);
    Displ_WString(10,10+Font12.Height*k++,text,font,1,BLACK,WHITE);
    sprintf(text,"Ay=%f By=%f",AY,BY);
    Displ_WString(10,10+Font12.Height*k++,text,font,1,BLACK,WHITE);
#ifdef  T_ROTATION_0
    sprintf(text,"Orientation 0");
#endif
#ifdef  T_ROTATION_90
    sprintf(text,"Orientation 90");
#endif
#ifdef  T_ROTATION_180
    sprintf(text,"Orientation 180");
#endif
#ifdef  T_ROTATION_270
    sprintf(text,"Orientation 270");
#endif
    Displ_WString(10,10+Font12.Height*k++,text,font,1,BLACK,WHITE);
    k++;
    sprintf(text,"Current test:");
    Displ_WString(10,10+font.Height*k++,text,font,1,BLACK,WHITE);
    sprintf(text,"Ax=%f Bx=%f",ax[0],bx[0]);
    Displ_WString(10,10+font.Height*k++,text,font,1,RED,WHITE);
    sprintf(text,"Ay=%f By=%f",ay[0],by[0]);
    Displ_WString(10,10+font.Height*k++,text,font,1,RED,WHITE);
    sprintf(text,"Ax=%f Bx=%f",ax[1],bx[1]);
    Displ_WString(10,10+font.Height*k++,text,font,1,BLUE,WHITE);
    sprintf(text,"Ay=%f By=%f",ay[1],by[1]);
    Displ_WString(10,10+font.Height*k++,text,font,1,BLUE,WHITE);
    k++;
    sprintf(text,"Proposed config:");
    Displ_WString(10,10+font.Height*k++,text,font,1,BLACK,WHITE);
    sprintf(text,"Ax=%f Bx=%f",axx,bxx);
    Displ_WString(10,10+font.Height*k++,text,font,1,DD_GREEN,WHITE);
    sprintf(text,"Ay=%f By=%f",ayy,byy);
    Displ_WString(10,10+font.Height*k++,text,font,1,DD_GREEN,WHITE);

    // Always show Orientation 90 for landscape
    sprintf(text,"Orientation 90");
    Displ_WString(10,10+font.Height*k++,text,font,1,DD_GREEN,WHITE);

    // Draw crosses to show calibration accuracy
    xx=(ax[0]*read_x[4]+bx[0]);
    yy=(ay[0]*read_y[4]+by[0]);
    DrawCross(xx,yy,RED);
    xx=(ax[1]*read_x[4]+bx[1]);
    yy=(ay[1]*read_y[4]+by[1]);
    DrawCross(xx,yy,BLUE);
    xx=(axx*read_x[4]+bxx);
    yy=(ayy*read_y[4]+byy);
    DrawCross(xx,yy,DD_GREEN);

    Touch_WaitForTouch(0);
}

void Read_Raw_Touch_Values(void) {
    char buffer[60];
    uint16_t raw_x, raw_y, raw_z;
    uint16_t x_display, y_display;
    uint8_t isTouch;

    Displ_CLS(BLACK);
    Displ_FillArea(0, 0, 320, 30, BLUE);
    Displ_WString(10, 8, "RAW TOUCH VALUE READER", Font16, 1, WHITE, BLUE);

    Displ_WString(10, 40, "Touch these 4 corners:", Font12, 1, YELLOW, BLACK);
    Displ_WString(10, 60, "1. Top-Left (20, 20)", Font12, 1, WHITE, BLACK);
    Displ_WString(10, 75, "2. Top-Right (300, 20)", Font12, 1, WHITE, BLACK);
    Displ_WString(10, 90, "3. Bottom-Right (300, 460)", Font12, 1, WHITE, BLACK);
    Displ_WString(10, 105, "4. Bottom-Left (20, 460)", Font12, 1, WHITE, BLACK);

    Displ_WString(10, 130, "Write down the RAW values!", Font12, 1, RED, BLACK);

    uint16_t test_points[4][2] = {
        {20, 20},      // Top-left
        {300, 20},     // Top-right
        {300, 460},    // Bottom-right
        {20, 460}      // Bottom-left
    };

    uint16_t raw_readings[4][2]; // Store raw X and Y for each corner

    for (int i = 0; i < 4; i++) {
        Displ_FillArea(0, 150, 320, 330, BLACK);

        // Draw crosshair
        DrawCross(test_points[i][0], test_points[i][1], RED);

        sprintf(buffer, "Touch Corner %d/4", i + 1);
        Displ_FillArea(0, 200, 320, 25, D_BLUE);
        Displ_WString(10, 205, buffer, Font16, 1, WHITE, D_BLUE);

        sprintf(buffer, "Target: X=%d Y=%d", test_points[i][0], test_points[i][1]);
        Displ_WString(10, 230, buffer, Font12, 1, CYAN, BLACK);

        // Wait for touch
        while (!Touch_GotATouch(0)) {
            HAL_Delay(10);
        }

        // Read RAW values DIRECTLY from sensor (bypassing calibration)
        Touch_Select();
        HAL_Delay(5);

        // Average multiple readings for accuracy
        uint32_t sum_x = 0, sum_y = 0, sum_z = 0;
        for (int j = 0; j < 10; j++) {
            sum_x += Touch_PollAxis(X_AXIS);
            sum_y += Touch_PollAxis(Y_AXIS);
            sum_z += Touch_PollAxis(Z_AXIS);
            HAL_Delay(5);
        }

        raw_x = sum_x / 10;
        raw_y = sum_y / 10;
        raw_z = sum_z / 10;

        Touch_UnSelect();

        // Store readings
        raw_readings[i][0] = raw_x;
        raw_readings[i][1] = raw_y;

        // Display RAW values
        Displ_FillArea(0, 255, 320, 60, D_GREEN);
        sprintf(buffer, "RAW X = %d", raw_x);
        Displ_WString(10, 260, buffer, Font16, 1, WHITE, D_GREEN);

        sprintf(buffer, "RAW Y = %d", raw_y);
        Displ_WString(10, 280, buffer, Font16, 1, WHITE, D_GREEN);

        sprintf(buffer, "RAW Z = %d", raw_z);
        Displ_WString(10, 300, buffer, Font12, 1, YELLOW, D_GREEN);

        // Wait for release
        Touch_WaitForUntouch(0);
        HAL_Delay(800);
    }

    // Calculate calibration coefficients
    Displ_CLS(BLACK);
    Displ_FillArea(0, 0, 320, 30, BLUE);
    Displ_WString(10, 8, "CALIBRATION RESULTS", Font16, 1, WHITE, BLUE);

    // Show all raw readings
    Displ_WString(10, 40, "Raw readings collected:", Font12, 1, YELLOW, BLACK);
    for (int i = 0; i < 4; i++) {
        sprintf(buffer, "Corner %d: X=%4d Y=%4d", i+1, raw_readings[i][0], raw_readings[i][1]);
        Displ_WString(10, 60 + i*18, buffer, Font12, 1, CYAN, BLACK);
    }

    // Calculate AX and BX using corners 0 (top-left) and 1 (top-right)
    // Display X = AX * Raw X + BX
    // At corner 0: 20 = AX * raw_readings[0][0] + BX
    // At corner 1: 300 = AX * raw_readings[1][0] + BX
    // Solving: AX = (300 - 20) / (raw_readings[1][0] - raw_readings[0][0])

    float ax_calc = (float)(test_points[1][0] - test_points[0][0]) /
                    (float)(raw_readings[1][0] - raw_readings[0][0]);
    float bx_calc = test_points[0][0] - (ax_calc * raw_readings[0][0]);

    // Calculate AY and BY using corners 0 (top-left) and 3 (bottom-left)
    // Display Y = AY * Raw Y + BY
    // At corner 0: 20 = AY * raw_readings[0][1] + BY
    // At corner 3: 460 = AY * raw_readings[3][1] + BY

    float ay_calc = (float)(test_points[3][1] - test_points[0][1]) /
                    (float)(raw_readings[3][1] - raw_readings[0][1]);
    float by_calc = test_points[0][1] - (ay_calc * raw_readings[0][1]);

    // Display calculated coefficients
    Displ_WString(10, 150, "COPY THESE VALUES:", Font16, 1, RED, BLACK);
    Displ_WString(10, 170, "to z_touch_XPT2046.h", Font12, 1, RED, BLACK);

    Displ_FillArea(0, 195, 320, 120, D_GREEN);

    sprintf(buffer, "#define AX %.6ff", ax_calc);
    Displ_WString(10, 200, buffer, Font12, 1, BLACK, D_GREEN);

    sprintf(buffer, "#define BX %.6ff", bx_calc);
    Displ_WString(10, 220, buffer, Font12, 1, BLACK, D_GREEN);

    sprintf(buffer, "#define AY %.6ff", ay_calc);
    Displ_WString(10, 240, buffer, Font12, 1, BLACK, D_GREEN);

    sprintf(buffer, "#define BY %.6ff", by_calc);
    Displ_WString(10, 260, buffer, Font12, 1, BLACK, D_GREEN);

    sprintf(buffer, "#define T_ROTATION_0");
    Displ_WString(10, 290, buffer, Font12, 1, BLACK, D_GREEN);

    // Verification info
    Displ_WString(10, 330, "Expected ranges:", Font12, 1, YELLOW, BLACK);
    Displ_WString(10, 345, "AX: 0.08-0.10", Font12, 1, WHITE, BLACK);
    Displ_WString(10, 360, "BX: -30 to 0", Font12, 1, WHITE, BLACK);
    Displ_WString(10, 375, "AY: 0.12-0.15", Font12, 1, WHITE, BLACK);
    Displ_WString(10, 390, "BY: -60 to -30", Font12, 1, WHITE, BLACK);

    // Check if values are reasonable
    if (ax_calc < 0.06 || ax_calc > 0.12) {
        Displ_WString(10, 410, "WARNING: AX looks wrong!", Font12, 1, RED, BLACK);
    }
    if (ay_calc < 0.10 || ay_calc > 0.18) {
        Displ_WString(10, 425, "WARNING: AY looks wrong!", Font12, 1, RED, BLACK);
    }

    while(1) {
        HAL_Delay(100);
    }
}

void Verify_Touch_Calibration(void) {
    char buffer[60];
    uint16_t x, y;
    uint8_t isTouch;

    Displ_CLS(BLACK);

    // Draw grid
    for (int i = 0; i <= 320; i += 32) {
        Displ_Line(i, 0, i, 479, DDDD_WHITE);
    }
    for (int j = 0; j <= 480; j += 48) {
        Displ_Line(0, j, 319, j, DDDD_WHITE);
    }

    Displ_FillArea(0, 0, 320, 25, BLUE);
    Displ_WString(10, 5, "Touch Verification", Font16, 1, WHITE, BLUE);

    // Draw test crosshairs at known positions
    DrawCross(20, 20, GREEN);
    DrawCross(300, 20, GREEN);
    DrawCross(300, 460, GREEN);
    DrawCross(20, 460, GREEN);
    DrawCross(160, 240, GREEN); // Center

    while(1) {
        if (Touch_GotATouch(2)) {
            Touch_GetXYtouch(&x, &y, &isTouch);

            if (isTouch) {
                // Check if values are reasonable
                if (x > 320 || y > 480) {
                    Displ_FillArea(0, 450, 320, 30, RED);
                    sprintf(buffer, "ERROR: X=%d Y=%d", x, y);
                    Displ_WString(10, 455, buffer, Font12, 1, WHITE, RED);
                } else {
                    // Draw touch point
                    Displ_fillCircle(x, y, 4, RED);

                    // Show coordinates
                    Displ_FillArea(0, 450, 320, 30, D_GREEN);
                    sprintf(buffer, "X=%3d Y=%3d", x, y);
                    Displ_WString(10, 455, buffer, Font16, 1, WHITE, D_GREEN);

                    // Check accuracy
                    if (abs(x - 20) < 10 && abs(y - 20) < 10) {
                        Displ_WString(180, 455, "Top-Left OK!", Font12, 1, GREEN, D_GREEN);
                    } else if (abs(x - 300) < 10 && abs(y - 20) < 10) {
                        Displ_WString(180, 455, "Top-Right OK!", Font12, 1, GREEN, D_GREEN);
                    } else if (abs(x - 300) < 10 && abs(y - 460) < 10) {
                        Displ_WString(180, 455, "Bot-Right OK!", Font12, 1, GREEN, D_GREEN);
                    } else if (abs(x - 20) < 10 && abs(y - 460) < 10) {
                        Displ_WString(180, 455, "Bot-Left OK!", Font12, 1, GREEN, D_GREEN);
                    } else if (abs(x - 160) < 10 && abs(y - 240) < 10) {
                        Displ_WString(180, 455, "Center OK!", Font12, 1, GREEN, D_GREEN);
                    }
                }
            }
        }
        HAL_Delay(50);
    }
}
