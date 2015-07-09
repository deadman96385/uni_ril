#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include "./minui/minui.h"
#include "common.h"

#include <cutils/android_reboot.h>
#include "./res/string_cn.h"
#include "ui.h"

void test_result_init(void)
{
	ui_init();
	LOGD("mmitest after ui_init\n");
	ui_set_background(BACKGROUND_ICON_NONE);
}

int test_lcd_start(void)
{
	int ret = 0;
	ui_fill_locked();
	ui_fill_screen(255, 255, 255);
	gr_flip();
	usleep(500*1000);

	ui_fill_screen(0, 0, 0);
	gr_flip();
	usleep(500*1000);

	ui_fill_screen(255, 0, 0);
	gr_flip();
	usleep(500*1000);

	ui_fill_screen(0, 255, 0);
	gr_flip();
	usleep(500*1000);

	ui_fill_screen(0, 0, 255);
	gr_flip();
	usleep(500*1000);



	ui_show_title(MENU_TEST_LCD);
	gr_color(255, 255, 255, 255);
	ui_show_text(3, 0, TEXT_FINISH);
	ui_set_color(CL_GREEN);
	ui_show_text(4, 0, LCD_TEST_TIPS);//+++++++++++++++++
	ret = ui_handle_button(TEXT_PASS, TEXT_FAIL);//, TEXT_GOBACK
	//save_result(CASE_TEST_LCD,ret);
	return ret;
}



