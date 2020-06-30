//#include"ray.h"
#include"func.h"
#include"camera.h"

#include"sphere.h"
#include"moving_sphere.h"
#include"hitablelist.h"
#include"bvh.h"

#include"material.h"
#include"texture.h"

#include<iostream>
#include<fstream>
#include<ctime>
#include<Windows.h>
using namespace std;

// 图片文件位置
#define FILENAME "D:\\DATA\\0.bmp"
// 线程数
#define NThread 24

typedef struct para_t {
	int index;
	RGBTRIPLE(*bm)[WIDTH];
	camera* cam;
	hitable* world;
}para_t;



vec3 color(const ray& r, hitable* world, int depth);
int render(RGBTRIPLE(*bm)[WIDTH], camera& cam, hitable* world);
DWORD WINAPI render_i(LPVOID para);

hitable* random_scene();
hitable* random_scene_1();
hitable* simple_scene();

int main() {
	if ((unsigned int)NThread > 64) {
		cerr << "Too many threads..." << endl;
		return -1;
	}
	//用于计时
	time_t time_start = time(0);
	
	// 输出文件的缓冲区
	RGBTRIPLE(*bm)[WIDTH];
	RGBTRIPLE* tmp = new RGBTRIPLE[HEIGHT * WIDTH * sizeof(RGBTRIPLE)];
	bm = (RGBTRIPLE(*)[WIDTH])tmp;
	//camera设置
	vec3 lookfrom(8, 8, 8);					// 视线起点
	vec3 lookat(0, 0, 0);						// 视线终点
	vec3 vup = vec3(0, 1, 0);					// 图片up方向
	double vfov = 20;							// Field of view
	double aspect = (double)WIDTH / HEIGHT;		// 长宽比
	double dist_to_focus = 10;					// 焦距 // 用于失焦模糊
	double aperture = 0;						// 光圈 //
	double t0 = 0;								// 起始时间 // 用于动态模糊
	double t1 = 1;								// 终止时间
	camera cam(lookfrom, lookat, vup, vfov, aspect, aperture, dist_to_focus, t0, t1);
	// 待绘制的场景
	hitable* scene = random_scene();
	// 开始绘制
	render(bm, cam, scene);
	// 初始化BMP文件头
	BITMAPFILEHEADER bf;
	BITMAPINFOHEADER bi;
	bf.bfType = 19778;
	bf.bfSize = WIDTH * HEIGHT * 3 + 54;
	bf.bfReserved1 = 0;
	bf.bfReserved2 = 0;
	bf.bfOffBits = 54;
	bi.biWidth = WIDTH;
	bi.biHeight = HEIGHT;
	bi.biSize = 40;
	bi.biPlanes = 1;
	bi.biBitCount = 24;
	bi.biCompression = BI_RGB;
	bi.biSizeImage = 0;
	bi.biXPelsPerMeter = 0;
	bi.biYPelsPerMeter = 0;
	bi.biClrUsed = 0;
	bi.biClrImportant = 0;
	// 打开输出文件
	fstream fout(FILENAME, ios::out | ios::binary);
	if (!fout.is_open()) {
		cout << "Cannot open file...";
		return -2;
	}
	// 输出到文件
	fout.write((const char*)&bf, sizeof(BITMAPFILEHEADER));
	fout.write((const char*)&bi, sizeof(BITMAPINFOHEADER));
	fout.write((const char*)bm, HEIGHT * WIDTH * sizeof(RGBTRIPLE));
	fout.close();
	cout << time(0) - time_start << "s" << endl;
	return 0;
}

DWORD WINAPI render_i(LPVOID para) {
	para_t s = *(para_t*)para;
	hitable* world = s.world;
	camera* cam = s.cam;

	RGBTRIPLE(*bm)[WIDTH] = s.bm;
	for (int i = HEIGHT * s.index / NThread; i < HEIGHT * (s.index + 1) / NThread; i++) {
		for (int j = 0; j < WIDTH; j++) {
			vec3 col(0, 0, 0);
			for (int s = 0; s < AA; s++) {
				double u = double(j + rand1()) / WIDTH;
				double v = double(i + rand1()) / HEIGHT;
				ray r = cam->get_ray(u, v);
				col += color(r, world, 0);
			}
			col /= AA;

			col = vec3(sqrt(col[0]), sqrt(col[1]), sqrt(col[2]));
			col *= 255.99;

			bm[i][j].rgbtRed = col.r();
			bm[i][j].rgbtGreen = col.g();
			bm[i][j].rgbtBlue = col.b();
		}
	}
	return 0;
}

vec3 color(const ray& r, hitable* world, int depth) {
	hit_record rec;
	if (world->hit(r, 0.001, MAXINT, rec)) {
		ray scattered;
		vec3 attenuation;
		if (depth < 50 && rec.mat_ptr->scatter(r, rec, attenuation, scattered)) {
			return attenuation * color(scattered, world, depth + 1);
		}
		else {
			return vec3(0, 0, 0);
		}
	}
	else {
		return vec3(0.8, 0.8, 0.8);
	}
}

int render(RGBTRIPLE(*bm)[WIDTH], camera& cam, hitable* world) {

	HANDLE hThread[NThread];
	para_t paras[NThread];
	for (int i = 0; i < NThread; i++) {
		paras[i].index = i;
		paras[i].cam = &cam;
		paras[i].world = world;
		paras[i].bm = bm;
		hThread[i] = CreateThread(0, 0, render_i, &paras[i], 0, 0);
	}
	WaitForMultipleObjects(NThread, hThread, 1, INFINITE);
	return 0;
}

hitable* random_scene() {
	texture* checker = new checker_texture(new constant_texture(vec3(0.2, 0.3, 0.1)), new constant_texture(vec3(0.9, 0.9, 0.9)));
	int n = 500;
	hitable** list = new hitable * [n];
	list[0] = new sphere(vec3(0, -1000, 0), 1000, new lambertian(checker));
	int i = 1;
	for (int a = -10; a < 10; a++) {
		for (int b = -10; b < 10; b++) {
			double choose_mat = rand1();
			vec3 center(a + 0.9 * rand1(), 0.2, b + 0.9 * rand1());
			if ((center - vec3(4, 0.2, 0)).squared_length() > 0.81) {
				if (choose_mat < 0.8) {
					list[i++] = new moving_sphere(center, center + vec3(0, 0.5 * rand1(), 0), 0, 1, 0.2, new lambertian(new constant_texture(vec3(rand1(), rand1(), rand1()))));
				}
				else if (choose_mat < 0.95) {
					list[i++] = new sphere(center, 0.2, new metal(vec3(0.5 * (1 + rand1()), 0.5 * (1 + rand1()), 0.5 * (1 + rand1())), 0.5 * rand1()));
				}
				else {
					list[i++] = new sphere(center, 0.2, new dielectric(1.5));
				}
			}
		}
	}
	list[i++] = new sphere(vec3(0, 1, 0), 1, new dielectric(1.5));
	list[i++] = new sphere(vec3(-4, 1, 0), 1, new lambertian(new constant_texture(vec3(0.4, 0.2, 0.1))));
	list[i++] = new sphere(vec3(4, 1, 0), 1, new metal(vec3(0.7, 0.6, 0.5), 0));
//	return new bvh_node(list, i, 0, 1);
	return new hitable_list(list, i);
}

hitable* random_scene_1() {
	texture* checker = new checker_texture(new constant_texture(vec3(0.2, 0.3, 0.1)), new constant_texture(vec3(0.9, 0.9, 0.9)));
	int n = 110;
	hitable** list = new hitable * [n];
	list[0] = new sphere(vec3(0, -1000, 0), 1000, new lambertian(checker));
	int i = 1;
	for (int a = -10; a < 10; a += 2) {
		for (int b = -10; b < 10; b += 2) {
			double choose_mat = rand1();
			vec3 center(a + 1.6 * rand1(), 0.4, b + 1.6 * rand1());
			if ((center - vec3(4, 0.4, 0)).squared_length() > 0.64) {
				if (choose_mat < 0.2) {
					list[i++] = new sphere(center, 0.4, new lambertian(new constant_texture(vec3(rand1(), rand1(), rand1()))));
				}
				else if (choose_mat < 0.9) {
					list[i++] = new sphere(center, 0.4, new metal(vec3(0.5 * (1 + rand1()), 0.5 * (1 + rand1()), 0.5 * (1 + rand1())), 0.5 * rand1()));
				}
				else {
					list[i++] = new sphere(center, 0.4, new dielectric(1.5));
				}
			}
		}
	}
	list[i++] = new sphere(vec3(0, 1, 0), 1, new dielectric(1.5));
	list[i++] = new sphere(vec3(-4, 1, 0), 1, new lambertian(new constant_texture(vec3(0.4, 0.2, 0.1))));
	list[i++] = new sphere(vec3(4, 1, 0), 1, new metal(vec3(0.7, 0.6, 0.5), 0));
	return new hitable_list(list, i);
}

hitable* simple_scene() {
	hitable** list = new hitable * [6];
	list[0] = new sphere(vec3(0, 0, 1), 1, new metal(vec3(0.5, 0.6, 0.7), 0.1));
	list[1] = new sphere(vec3(1.5, 0, 0.4), 0.4, new lambertian(new constant_texture(vec3(0.8, 0.7, 0.2))));
	list[2] = new sphere(vec3(0, 1.5, 0.4), 0.4, new lambertian(new constant_texture(vec3(0.2, 0.3, 0.8))));
	list[3] = new sphere(vec3(0, -1.5, 0.4), 0.4, new lambertian(new constant_texture(vec3(0.4, 0.1, 0.2))));
	list[4] = new sphere(vec3(-1.5, 0, 0.4), 0.4, new dielectric(1.4));
	list[5] = new sphere(vec3(0, 0, -10000), 10000, new lambertian(new checker_texture(new constant_texture(vec3(0.2, 0.3, 0.1)), new constant_texture(vec3(0.9, 0.9, 0.9)))));
	return new hitable_list(list, 6);
}