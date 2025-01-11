#define _CRT_SECURE_NO_WARNINGS
#include<ctype.h>
#include<SDL.h>
#include<SDL_audio.h>
#include<SDL_mixer.h>
#include<stdbool.h>
#include<stdlib.h>
#include<stdint.h>
#include<stdio.h>
#include<string.h>
#include<time.h>

#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define NORMAL "\x1b[m"
#define PRINT_FUNC printf("%s: ", __func__)
#define TEST 0
#define FRAME_RATE 60

const int SCREEN_WIDTH = 1280;
const int SCREEN_HEIGHT = 640;

SDL_Window* window = NULL;

SDL_Surface* screenSurface = NULL;

SDL_Renderer* renderer = NULL;

Mix_Chunk* beep = NULL;

SDL_Event e;

//memory
uint8_t memory[0x1000] = { 0 };
uint8_t* str = &memory[0x1001];

//cpu
uint8_t V[0x10] = { 0 };

//timer registers
uint8_t dt = 0;
uint8_t st = 0;

//i register
uint16_t I = 0;

//program counter
uint16_t pc = 0;

//stack pointer
typedef struct {
	//use bitfield to limit to 2^4 values i.e. overflows after 15 to prevent access outside of stack
	uint8_t value : 4;
} sptr;

sptr sp  = { 0 };

//stack
uint16_t stack[0x10] = { 0 };

SDL_KeyCode keys[0x10] =   {SDLK_x, SDLK_1, SDLK_2, SDLK_3, SDLK_q, SDLK_w, SDLK_e, SDLK_a,
							SDLK_s, SDLK_d, SDLK_z, SDLK_c, SDLK_4, SDLK_r, SDLK_f, SDLK_v};

bool keypad[0x10] = { 0 };

//since display is monochrome, can use one bit for each pixel. 64(/8)x32 pixels
uint8_t display[8][32] = { 0 };

//0-F ascii sprites;
uint8_t font[80]={  0xf0, 0x90, 0x90, 0x90, 0xf0,
					0x20, 0x60, 0x20, 0x20, 0x70,
					0xf0, 0x10, 0xf0, 0x80, 0xf0,
					0xf0, 0x10, 0xf0, 0x10, 0xf0,
					0x90, 0x90, 0xf0, 0x10, 0x10,
					0xf0, 0x80, 0xf0, 0x10, 0xf0,
					0xf0, 0x80, 0xf0, 0x90, 0xf0,
					0xf0, 0x10, 0x20, 0x40, 0x40,
					0xf0, 0x90, 0xf0, 0x90, 0xf0,
					0xf0, 0x90, 0xf0, 0x10, 0xf0,
					0xf0, 0x90, 0xf0, 0x90, 0x90,
					0xe0, 0x90, 0xe0, 0x90, 0xe0,
					0xf0, 0x80, 0x80, 0x80, 0xf0,
					0xe0, 0x90, 0x90, 0x90, 0xe0,
					0xf0, 0x80, 0xf0, 0x80, 0xf0,
					0xf0, 0x80, 0xf0, 0x80, 0x80 };

bool init();
bool load_rom(char* path, char* type);
void draw_frame();
void close();

//instruction set
void do_op(uint16_t op);
void CLS();
void RET();
void JP(uint16_t op);
void CALL(uint16_t op);
void SEVX(uint16_t op);
void SNEVX(uint16_t op);
void SEVXVY(uint16_t op);
void LDVX(uint16_t op);
void ADD(uint16_t op);
void LDVXVY(uint16_t op);
void OR(uint16_t op);
void AND(uint16_t op);
void XOR(uint16_t op);
void ADDCARRY(uint16_t op);
void SUB(uint16_t op);
void SHR(uint16_t op);
void SUBN(uint16_t op);
void SHL(uint16_t op);
void SNEVXVY(uint16_t op);
void LDI(uint16_t op);
void JPV0(uint16_t op);
void RND(uint16_t op);
void DRW(uint16_t op);
void SKPVX(uint16_t op);
void SKNPVX(uint16_t op);
void LDVXDT(uint16_t op);
void LDVXK(uint16_t op);
void LDDTVX(uint16_t op);
void LDSTVX(uint16_t op);
void ADDIVX(uint16_t op);
void LDFVX(uint16_t op);
void LDBVX(uint16_t op);
void LDIVX(uint16_t op);
void LDVXI(uint16_t op);
void PASS(uint16_t op);
//handlers for op codes that share common first nibble
//op param maybe unnecessary for 0x0xxx
void op_0x0xxx(uint16_t op);
void op_0x8xxx(uint16_t op);
void op_0xExxx(uint16_t op);
void op_0xFxxx(uint16_t op);

//function pointer array for do_op
void(*op_0x8xyn[16])() = { &LDVXVY, &OR, &AND, &XOR, &ADDCARRY, &SUB, &SHR, &SUBN, &PASS, &PASS, &PASS, &PASS, &PASS, &PASS, &SHL, &PASS};
void(*op_0xnxxx[16])() = {&op_0x0xxx, &JP, &CALL, &SEVX, &SNEVX, &SEVXVY, &LDVX, &ADD, &op_0x8xxx, &SNEVXVY, &LDI, &JPV0, &RND, &DRW, &op_0xExxx, &op_0xFxxx};

#if (TEST == 1)
	bool init_test();
	bool JP_test();
	bool CALL_test();
	bool SEVX_test();
	bool SNEVX_test();
	bool SEVXVY_test();
	bool LDVX_test();
	bool ADD_test();
	bool LDVXVY_test();
	bool OR_test();
	bool AND_test();
	bool XOR_test();
	bool ADDCARRY_test();
	bool SUB_test();
	bool SHR_test();
	bool SUBN_test();
	bool SHL_test();
	bool SNEVXVY_test();
	bool LDI_test();
	bool JPV0_test();
	bool RND_test();
	bool DRW_test();
	bool SKPVX_test();
	bool SKNPVX_test();
	bool LDVXDT_test();
	bool LDVXK_test();
	bool LDDTVX_test();
	bool LDSTVX_test();
	bool ADDIVX_test();
	bool LDFVX_test();
	bool LDBVX_test();
	bool LDIVX_test();
	bool LDVXI_test();

	bool (*tests[])() = { &init_test, &LDVX_test, &JP_test, &CALL_test, &SEVX_test, &SNEVX_test, &SEVXVY_test, &ADD_test, &OR_test, &AND_test, &XOR_test, &ADDCARRY_test,
						  &SUB_test, &SHR_test, &SUBN_test, &SHR_test, &SNEVXVY_test, &LDI_test, &JPV0_test, &RND_test, &DRW_test, &SKPVX_test, &SKNPVX_test, &LDVXDT_test,
						  &LDVXK_test, &LDDTVX_test, &LDSTVX_test, &ADDIVX_test, &LDFVX_test, &LDBVX_test, &LDIVX_test, &LDVXI_test, NULL };
#endif
int main(int argc, char* argv[])
{
#if (TEST == 1)
	int fails = 0;
	int passes = 0;
	for (int i = 0; tests[i] != NULL; i++) {
		if (tests[i]()) {
			passes++;
			printf("%spassed!%s\n", GREEN, NORMAL);
		}
		else {
			fails++;
			printf("%sfailed!%s\n", RED, NORMAL);
		}
	}
	printf("%spassed %i tests.%s\n", GREEN, passes, NORMAL);
	printf("%sfailed %i tests.%s\n", RED, fails, NORMAL);
	close();
	return 0;
#else
	/*
	if (argc < 3) {
		printf("Error: usage is 'skibidi-emu [path] [normal/ETI]' \n");
		return 1;
	}
	
	{
		//neat scope trick, len goes out of scope after use
		int len = (int)strlen(argv[2]);
		for (int i = 0; i < len; i++) {
			argv[2][i] = toupper(argv[2][i]);
		}
	}
	*/
	if (!init()) {
		return 1;
	}
	printf("initialization successful.\n");
	if (!load_rom("roms/hello.ch8", "normal")) {
		printf("Could not load ROM at '%s'. Check path.\n", argv[1]);
		goto err_close;
	}
	printf("load successful.\n");
	struct timespec frame_start = { 0, 0 };
	struct timespec frame_end = { 0, 0 };
	bool quit = false;
	while (quit == false) {
		if (!timespec_get(&frame_start, TIME_UTC)) {
			printf("Timing error at frame_start.\n");
			goto err_close;
		}
		//decrement timers
		if (st) {
			if (!Mix_Playing(0)) {
				Mix_PlayChannelTimed(0, beep, 0, 1000 * st / FRAME_RATE);
			}
			st--;
		}
		if (dt) {
			dt -= 1;
		}
		while ((frame_end.tv_sec * 1000000000 + frame_end.tv_nsec) - (frame_start.tv_sec * 1000000000 + frame_start.tv_nsec) < 1000000000 / FRAME_RATE) {
			if (SDL_PollEvent(&e) != 0) {
				if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
					for (int i = 0; i < 0x10; i++) {
						if (e.key.keysym.sym == keys[i]) {
							if (e.type == SDL_KEYDOWN) {
								keypad[i] = 1;
								break;
							}
							else keypad[i] = 0;
							break;
						}
					}
				}
				if (e.type == SDL_QUIT) {
					quit = 1;
				}
			}

			//do ops
			if (pc > 0xFFE) {
				printf("Program counter reached the end of the ROM.\n");
				goto err_close;
			}
			//printf("doing op %02X%02X pc %i\n", memory[pc], memory[pc + 1], pc); this was a debug line
			do_op((memory[pc] << 8) + memory[pc + 1]);
			pc += 2;

			if (!((memory[pc - 2] ^ 0xD0) & 0xF0)) {
				//wait until end of frame and draw new frame
				do {
					if (!timespec_get(&frame_end, TIME_UTC)) {
						printf("Timing error at frame_end.\n");
						goto err_close;
					}
				} while ((frame_end.tv_sec * 1000000000 + frame_end.tv_nsec) - (frame_start.tv_sec * 1000000000 + frame_start.tv_nsec) < 1000000000 / FRAME_RATE);
				draw_frame();
				break;
			}

			if (!timespec_get(&frame_end, TIME_UTC)) {
				printf("Timing error at frame_end.\n");
				goto err_close;
			}			
		}
	}
	close();
	printf("Starting at memory[%i] - %s\n", str - &memory, str);
	return 0;
err_close:
	close();
	return 1;
#endif
}

	void do_op(uint16_t op) {
		op_0xnxxx[(op & 0xF000) >> 12](op);
}

void CLS() {
	for (uint8_t i = 0; i < 8; i++) {
		for (uint8_t j = 0; j < 32; j++) {
			display[i][j] = 0x00;
		}
	}
}

void RET() {
	pc = stack[sp.value--];
}

void JP(uint16_t op) {
	pc = (op & 0x0FFF) - 2;
}

void CALL(uint16_t op) {
	stack[++sp.value] = pc;
	pc = (op & 0x0FFF) - 2;
}

void SEVX(uint16_t op) {
	if (V[(op & 0x0F00) >> 8] == (op & 0x00FF)) {
		pc += 2;
	}
}

void SNEVX(uint16_t op) {
	if (V[(op & 0x0F00) >> 8] != (op & 0x00FF)) {
		pc += 2;
	}
}

void SEVXVY(uint16_t op) {
	if (!(op & 0x000F)) {
		if (V[(op & 0x0F00) >> 8] == V[(op & 0x00F0) >> 4]) {
			pc += 2;
		}
	}
}

void LDVX(uint16_t op) {
	V[(op & 0x0F00) >> 8] = op & 0x00FF;
}

void ADD(uint16_t op) {
	V[(op & 0x0F00) >> 8] += op & 0x00FF;
}

void LDVXVY(uint16_t op) {
	V[(op & 0x0F00) >> 8] = V[(op & 0x00F0) >> 4];
}

void OR(uint16_t op) {
	V[(op & 0x0F00) >> 8] |= V[(op & 0x00F0) >> 4];
}

void AND(uint16_t op) {
	V[(op & 0x0F00) >> 8] &= V[(op & 0x00F0) >> 4];
}

void XOR(uint16_t op) {
	V[(op & 0x0F00) >> 8] ^= V[(op & 0x00F0) >> 4];
}

void ADDCARRY(uint16_t op) {
	V[(op & 0x0F00) >> 8] += V[(op & 0x00F0) >> 4];
	if (V[(op & 0x0F00) >> 8] += V[(op & 0x00F0) >> 4] < V[(op & 0x00F0) >> 4]) {
		V[0xF] = 0x01;
	}
	else {
		V[0xF] = 0;
	}
}

void SUB(uint16_t op) {
	if (V[(op & 0x0F00) >> 8] > V[(op & 0x00F0) >> 4]) {
		V[0xF] = 0x01;
	}
	else {
		V[0xF] = 0;
	}
	V[(op & 0x0F00) >> 8] -= V[(op & 0x00F0) >> 4];
}

void SHR(uint16_t op) {
	if (V[(op & 0x0F00) >> 8] & 0x0001) {
		V[0xF] = 0x01;
	}
	else {
		V[0xF] = 0;
	}
	V[(op & 0x0F00) >> 8] >>= 1;
}

void SUBN(uint16_t op) {
	if (V[(op & 0x0F00) >> 8] < V[(op & 0x00F0) >> 4]) {
		V[0xF] = 0x01;
	}
	else {
		V[0xF] = 0;
	}
	V[(op & 0x0F00) >> 8] = V[(op & 0x00F0) >> 4] - V[(op & 0x0F00) >> 8];
}

void SHL(uint16_t op) {
	if (V[(op & 0x0F00) >> 8] & 0x8000) {
		V[0xF] = 0x01;
	}
	else {
		V[0xF] = 0;
	}
	V[(op & 0x0F00) >> 8] <<= 1;
}

void SNEVXVY(uint16_t op) {
	if (!(op & 0x000F)) {
		//equivalent to if !(vx^vy)
		if (V[(op & 0x0F00) >> 8] != V[(op & 0x00F0) >> 4]) {
			pc += 2;
		}
	}
}

void LDI(uint16_t op) {
	I = op & 0x0FFF;
}

void JPV0(uint16_t op) {
	pc = V[0] + (op & 0x0FFF) - 2;
}

void RND(uint16_t op) {
	V[(op & 0x0F00) >> 8] = rand() & op & 0x00FF;
}

//must pass copy of op
void DRW(uint16_t op) {
	V[0xF] = 0;
#define COORD_X (V[(op & 0x0F00) >> 8] >> 3)
#define COORD_Y (V[(op & 0x00F0) >> 4])
#define N (op & 0x000F)
	while (N) {
		op--;
		if (display[COORD_X & 0x7][(COORD_Y + N) & 0x1f] & (memory[I + N] >> (V[(op & 0x0F00) >> 8] - (COORD_X << 3)))) {
			V[0xF] = 0x01;
		}
		display[COORD_X & 0x7][(COORD_Y + N) & 0x1f] ^= memory[I + N] >> (V[(op & 0x0F00) >> 8] - (COORD_X << 3));
		// & with 0x07 to constrain to 3 bits for wrapping
		if (display[(COORD_X + 1) & 0x07][(COORD_Y + N) & 0x1f] & (memory[I + N] << (8 - (V[(op & 0x0F00) >> 8] - (COORD_X << 3))))) {
			V[0xF] = 0x01;
		}
		display[(COORD_X + 1) & 0x07][(COORD_Y + N) & 0x1f] ^= memory[I + N] << (8 - (V[(op & 0x0F00) >> 8] - (COORD_X << 3)));
	}
#undef COORD_X
#undef COORD_Y
#undef N
}

void SKPVX(uint16_t op) {
	if (keypad[V[(op & 0x0F00) >> 8]&0xF]) {
		pc += 2;
	}
}

void SKNPVX(uint16_t op) {
	if (!keypad[V[(op & 0x0F00) >> 8]&0xF]) {
		pc += 2;
	}
}

void LDVXDT(uint16_t op) {
	V[(op & 0x0F00) >> 8] = dt;
}

//"All execution stops until a key is pressed, then the value of that key is stored in Vx."
//unclear if a key is already pressed down if it should be stored
void LDVXK(uint16_t op) {
	while (e.type != SDL_QUIT) {
		SDL_PollEvent(&e);
		if (e.type == SDL_KEYDOWN) {
			for (uint8_t i = 0; i < 0x10; i++) {
				if (keys[i] == e.key.keysym.sym) {
					V[(op & 0x0F00) >> 8] = i;
					return;
				}
			}
		}
	}
}

void LDDTVX(uint16_t op) {
	dt = V[(op & 0x0F00) >> 8];
}

void LDSTVX(uint16_t op) {
	st = V[(op & 0x0F00) >> 8];
}

void ADDIVX(uint16_t op) {
	I += V[(op & 0x0F00) >> 8];
}

//bitwise and with F to map to one of the 16 hex sprites loaded from 0 to 79
void LDFVX(uint16_t op) {
	I = 5 * (V[(op & 0x0F00) >> 8] & 0xF);
}

void LDBVX(uint16_t op) {
	memory[I] = V[(op & 0x0F00) >> 8] / 100;
	memory[I + 1] = (V[(op & 0x0F00) >> 8] % 100) / 10;
	memory[I + 2] = (V[(op & 0x0F00) >> 8]) % 10;
}

void LDIVX(uint16_t op) {
	do {
		memory[I + ((op & 0x0F00) >> 8)] = V[(op & 0x0F00) >> 8];
		op -= 0x100;
	} while ((op + 0x100) & 0xf00);
}

void LDVXI(uint16_t op) {
	do {
		V[(op & 0x0F00) >> 8] = memory[I + ((op & 0x0F00) >> 8)];
		op -= 0x0100;
	} while ((op + 0x100) & 0x0F00);
}

void PASS(uint16_t op) {
	return;
}

void op_0x0xxx(uint16_t op) {
	if (!(op ^ 0x00EE)) {
		RET();
	}
	else if (!(op ^ 0x00E0)) {
		CLS();
	}
	else PASS(op);
}

void op_0x8xxx(uint16_t op) {
		op_0x8xyn[op & 0x000F](op);
}

void op_0xExxx(uint16_t op) {
	if (!(op & ~0xEF9E)) {
		SKPVX(op);
	}
	else if (!(op & ~0xEFA1)) {
		SKNPVX(op);
	}
}

void op_0xFxxx(uint16_t op) {
	if (!(op & ~0xFF07)) {
		LDVXDT(op);
	}
	else if (!(op & ~0xFF0A)) {
		LDVXK(op);
	}
	else if (!(op & ~0xFF15)) {
		LDDTVX(op);
	}
	else if (!(op & ~0xFF18)) {
		LDSTVX(op);
	}
	else if (!(op & ~0xFF1E)) {
		ADDIVX(op);
	}
	else if (!(op & ~0xFF29)) {
		LDFVX(op);
	}
	else if (!(op & ~0xFF33)) {
		LDBVX(op);
	}
	else if (!(op & ~0xFF55)) {
		LDIVX(op);
	}
	else if (!(op & ~0xFF65)) {
		LDVXI(op);
	}
}

bool init() {
	bool success = true;
	//memory
	for (int i = 0; i < 0x1000; i++) {
		memory[i] = 0;
	}
	//load ascii 0-F sprites
	for (int i = 0; i < 0x50; i++) {
		memory[i] = font[i];
	}
	//seed randomness
	srand((unsigned int)time(NULL));
	//call rand once since the first value is very predictable based on system time
	rand();
	//registers
	for (int i = 0; i < 16; i++) {
		V[i] = 0;
	}
	dt = 0;
	st = 0;
	I = 0;
	pc = 0;
	sp.value = 0;
	//SDL
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
		printf("Could not initialize! Error: %s\n", SDL_GetError());
		success = false;
	}
	else {
		if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1")) {
			printf("warning: linear texture filtering not enables!\n");
		}
		window = SDL_CreateWindow("Skibidi", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
		if (window == NULL) {
			printf("Window could not be created! Error: %s\n", SDL_GetError());
			success = false;
		}
		else {
			renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
			if (renderer == NULL) {
				printf("Renderer could not be created! Error: %s\n", SDL_GetError());
				success = false;
			}
			else {
				SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
			}
			if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 1, 2048) < 0) {
				printf("SDL_mixer could not initialize! Error: %s\n", Mix_GetError());
				success = false;
			}
			beep = Mix_LoadWAV("Audio/beep.wav");
			if (beep == NULL) {
				printf("Failed to load beep. Error: %s", Mix_GetError());
				success = false;
			}
		}
	}
	return success;
}

bool load_rom(char* path, char* type) {
	uint16_t addr;
	if (type == "ETI") {
		addr = 0x600;
		pc = addr;
	}
	else {
		addr = 0x200;
		pc = addr;
	}
	FILE* rom = fopen(path, "rb");
	if (rom == NULL) {
		printf("file could not be either found or opened.\n");
		return false;
	}
	while (fread(memory + addr, 1, 1, rom)) {
		addr++;
	}
	printf("rom at %s loaded.\n", path);
	/*for (int i = pc; i < 0x1000; i += 2) {
		printf("%i %i%i\n", i, memory[i], memory[i + 1]);
	}
	*/
	fclose(rom);
	return true;
}

void draw_frame() {
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
	SDL_Rect background = { 0,0, SCREEN_WIDTH, SCREEN_HEIGHT };
	SDL_RenderFillRect(renderer, &background);
	SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
	SDL_Rect pixel = { 0, 0, SCREEN_WIDTH / 64, SCREEN_HEIGHT / 32 };
	for (int j = 0; j < 32; j++) {
		for (int i = 0; i < 8; i++) {
			for (int bit = 0; bit < 8; bit++) {
				if ((0x80 >> bit) & display[i][j]) {
					pixel.x = i * SCREEN_WIDTH / 8 + bit * SCREEN_WIDTH / 64;
					pixel.y = j * SCREEN_HEIGHT / 32;
					SDL_RenderFillRect(renderer, &pixel);
				}
			}
		}
	}
	SDL_RenderPresent(renderer);
}

void close() {
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	Mix_FreeChunk(beep);
	renderer = NULL;
	window = NULL;
	beep = NULL;

	SDL_AudioQuit();
	SDL_Quit();
}

//tests
#if (TEST == 1)

	bool init_test() {
		PRINT_FUNC;
		init();
		for (int i = 0; i < 80; i++) {
			if (memory[i] ^ font[i]) return false;
		}
		for (int i = 80; i < 0x1000; i++) {
			if (memory[i]) return false;
		}
		for (int i = 0; i < 0x1; i++) {
			if (V[i]) return false;
		}
		if (dt) return false;
		if (st) return false;
		if (I) return false;
		if (pc) return false;
		if (sp.value) return false;
		return true;
	}
	bool LDVX_test() {
		PRINT_FUNC;
		do_op(0x6789);
		if (V[7] != 0x89) return false;
		return true;
	}
	bool JP_test() {
		PRINT_FUNC;
		do_op(0x1234);
		if (pc != 0x234 - 2) return false;
		return true;
	}
	bool CALL_test() {
		PRINT_FUNC;
		uint8_t old_sp = sp.value;
		uint16_t old_pc = pc;
		do_op(0x2345);
		if (sp.value != old_sp + 1)	return false;
		if (stack[sp.value] != old_pc) return false;
		if (pc != 0x345 - 2) return false;
		return true;
	}
	bool SEVX_test() {
		PRINT_FUNC;
		uint16_t old_pc = pc;
		do_op(0x6455);
		do_op(0x3456);
		if (pc != old_pc) return false;
		do_op(0x6456);
		do_op(0x3456);
		if (pc != old_pc + 2) return false;
		return true;
	}
	bool SNEVX_test() {
		PRINT_FUNC;
		uint16_t old_pc = pc;
		do_op(0x6567);
		do_op(0x4567);
		if (pc != old_pc) return false;
		do_op(0x6566);
		do_op(0x4567);
		if (pc != old_pc + 2) return false;
		return true;
	}
	bool SEVXVY_test() {
		PRINT_FUNC;
		uint16_t old_pc = pc;
		do_op(0x6612);
		do_op(0x6713);
		do_op(0x5670);
		if (pc != old_pc) return false;
		do_op(0x6712);
		do_op(0x5678);
		if (pc != old_pc) return false;
		do_op(0x5670);
		if (pc != old_pc + 2) return false;
		return true;
	}
	bool ADD_test() {
		PRINT_FUNC;
		do_op(0x6810);
		do_op(0x789a);
		if (V[8] != 0x10 + 0x9a) return false;
		return true;
	}
	bool LDVXVY_test() {
		PRINT_FUNC;
		do_op(0x6900);;
		do_op(0x6a01);
		do_op(0x89a0);
		if (V[9] != V[0xa]) return false;
		return true;
	}
	bool OR_test() {
		PRINT_FUNC;
		do_op(0x6003);
		do_op(0x6104);
		do_op(0x8011);
		if (V[0] != 0x7) return false;
		do_op(0x6003);
		do_op(0x6103);
		do_op(0x8011);
		if (V[0] != 0x3) return false;
		do_op(0x6003);
		do_op(0x6101);
		do_op(0x8011);
		if (V[0] != 0x3) return false;
		return true;
	}
	bool AND_test() {
		PRINT_FUNC;
		do_op(0x6003);
		do_op(0x6104);
		do_op(0x8012);
		if (V[0] != 0x0) return false;
		do_op(0x6003);
		do_op(0x6103);
		do_op(0x8012);
		if (V[0] != 0x3) return false;
		do_op(0x6003);
		do_op(0x6101);
		do_op(0x8012);
		if (V[0] != 0x1) return false;
		return true;
	}
	bool XOR_test() {
		PRINT_FUNC;
		do_op(0x6003);
		do_op(0x6104);
		do_op(0x8013);
		if (V[0] != 0x7) return false;
		do_op(0x6003);
		do_op(0x6103);
		do_op(0x8013);
		if (V[0] != 0x0) return false;
		do_op(0x6003);
		do_op(0x6101);
		do_op(0x8013);
		if (V[0] != 0x2) return false;
		return true;
	}
	bool ADDCARRY_test() {
		PRINT_FUNC;
		do_op(0x60ff);
		do_op(0x61ff);
		do_op(0x8014);
		if (V[0] != 0xfe || V[0xf] != 1) return false;
		do_op(0x6010);
		do_op(0x6110);
		do_op(0x8014);
		if (V[0] != 0x20 || V[0xf] != 0) return false;
		return true;
	}
	bool SUB_test() {
		PRINT_FUNC;
		do_op(0x6010);
		do_op(0x6108);
		do_op(0x8015);
		if (V[0] != 0x8 || V[0xf] != 1) return false;
		do_op(0x6000);
		do_op(0x6101);
		do_op(0x8015);
		if (V[0] != 0xff || V[0xf] != 0) return false;
		return true;
	}
	bool SHR_test() {
		PRINT_FUNC;
		do_op(0x60ff);
		do_op(0x8006);
		if (V[0] != 0x7f || V[0xf] != 1) return false;
		do_op(0x6010);
		do_op(0x8006);
		if (V[0] != 0x08 || V[0xf] != 0) return false;
		return true;
	}
	bool SUBN_test() {
		PRINT_FUNC;
		do_op(0x6010);
		do_op(0x6120);
		do_op(0x8017);
		if (V[0] != 0x10 || V[0xf] != 1) return false;
		do_op(0x610f);
		do_op(0x8017);
		if (V[0] != 0xff || V[0xf] != 0) return false;
		return true;
	}
	bool SHL_test() {
		PRINT_FUNC;
		do_op(0x60ff);
		do_op(0x800e);
		if (V[0] != 0xfe || V[0xf] != 1) return false;
		do_op(0x6001);
		do_op(0x800e);
		if (V[0] != 0x2 || V[0xf] != 0) return false;
		return true;
	}
	bool SNEVXVY_test() {
		PRINT_FUNC;
		uint16_t old_pc = pc;
		do_op(0x6010);
		do_op(0x6110);
		do_op(0x9010);
		if (pc != old_pc) return false;
		do_op(0x6011);
		do_op(0x9011);
		if (pc != old_pc) return false;
		do_op(0x9010);
		if (pc != old_pc + 2) return false;
		return true;
	}
	bool LDI_test() {
		PRINT_FUNC;
		do_op(0xa123);
		if (I != 0x123) return false;
		return true;
	}
	bool JPV0_test() {
		PRINT_FUNC;
		do_op(0x6010);
		do_op(0xb123);
		if (pc != 0x10 + 0x123 - 0x2) return false;
		return true;
	}
	//can reasonably assume 10 calls to this will not result in the same number 10 times
	bool RND_test() {
		PRINT_FUNC;
		do_op(0xc010);
		if (V[0] > 16) return false;
		uint8_t rands[10];
		for (int i = 0; i < 10; i++) {
			do_op(0xc0ff);
			rands[i] = V[0];
		}
		for (int i = 0; i < 9; i++) {
			for (int j = i + 1; j < 10; j++) {
				if (rands[i] != rands[j]) {
					return true;
				}
			}
		}
		//if you generated the same number 10 times (1/2^80) go buy a lottery ticket
		return false;
	}
	bool DRW_test() {
		PRINT_FUNC;
		//draw sprites correctly
		for (int i = 0; i < 16; i++) {
			do_op(0x6000 + i * 8);
			do_op(0x6100 + (i / 8)*5);
			do_op(0xa000 + 5 * i);
			do_op(0xd015);
		}
		for (int i = 0; i < 16; i++) {
			for (int j = (i/8)*5; j < ((i/8) + 1) * 5 ; j++) {
				if (display[i & 0x7][j] != font[i * 5 + j % 5] || V[0xf] != 0) {
					return false;
				}
			}
		}
		//clear screen correctly
		do_op(0x00e0);
		for (int i = 0; i < 8; i++) {
			for (int j = 0; j < 32; j++) {
				if (display[i][j]) return false;
			}
		}
		//test xor
		do_op(0x6000);
		do_op(0x6100);
		do_op(0xa000);
		do_op(0xd015);
		do_op(0xd015);
		if (!V[0xf]) return false;
		for (int i = 0; i < 8; i++) {
			for (int j = 0; j < 32; j++) {
				if (display[i][j]) return false;
			}
		}
		do_op(0xd015);
		do_op(0xa000 + 5 * 3);
		do_op(0xd015);
		if (!V[0xf]) return false;
		if (display[0][0]) return false;
		if (display[0][1] != 0x80) return false;
		if (display[0][2] != 0x60) return false;
		if (display[0][3] != 0x80) return false;
		if (display[0][4]) return false;
		do_op(0x00e0);
		do_op(0x603e);
		do_op(0x611e);
		do_op(0xa000);
		do_op(0xd015);
		if (display[7][30] != font[0] >> 6 || display[0][30] != ((font[0] << 2) & 0xff)) return false;
		if (display[7][31] != font[1] >> 6 || display[0][31] != ((font[1] << 2) & 0xff)) return false;
		if (display[7][0] != font[2] >> 6 || display[0][0] != ((font[2] << 2) & 0xff)) return false;
		if (display[7][1] != font[3] >> 6 || display[0][1] != ((font[3] << 2) & 0xff)) return false;
		if (display[7][2] != font[4] >> 6 || display[0][2] != ((font[4] << 2) & 0xff)) return false;
		return true;
	}
	bool SKPVX_test() {
		PRINT_FUNC;
		uint16_t old_pc = pc;
		do_op(0x6000);
		do_op(0xe09e);
		if (pc != old_pc) return false;
		keypad[0] = true;
		do_op(0x6101);
		do_op(0xe19e);
		if (pc != old_pc) {
			keypad[0] = 0;
			return false;
		}
		do_op(0xe09e);
		keypad[0] = 0;
		if (pc != old_pc + 2) return false;
		return true;
	}
	bool SKNPVX_test() {
		PRINT_FUNC;
		uint16_t old_pc = pc;
		do_op(0x6000);
		keypad[0] = true;
		do_op(0xe0a1);
		if (pc != old_pc) return false;
		do_op(0x6101);
		keypad[1] = true;
		do_op(0xe1a1);
		if (pc != old_pc) return false;
		keypad[1] = 0;
		keypad[0] = 0;
		do_op(0xe0a1);
		if (pc != old_pc + 2) return false;
		return true;
	}
	bool LDVXDT_test() {
		PRINT_FUNC;
		do_op(0x6000);
		dt = 0xff;
		do_op(0xf007);
		if (V[0] != 0xff) return false;
		return true;
	}
	bool LDVXK_test() {
		PRINT_FUNC;
		//tests only for key press x, corresponding to key 0
		do_op(0xf00a);
		if (V[0] != 0) return false;
		return true;
	}
	bool LDDTVX_test() {
		PRINT_FUNC;
		do_op(0x60ff);
		do_op(0xf015);
		if (dt != 0xff) return false;
		return true;
	}
	bool LDSTVX_test() {
		PRINT_FUNC;
		do_op(0x60ff);
		do_op(0xf018);
		if (st != 0xff) return false;
		return true;
	}
	bool ADDIVX_test() {
		PRINT_FUNC;
		do_op(0x600f);
		do_op(0xa00f);
		do_op(0xf01e);
		if (I != 0x1e) return false;
		return true;
	}
	bool LDFVX_test() {
		PRINT_FUNC;
		do_op(0x600f);
		do_op(0xf029);
		if (I != 5 * 0xf) return false;
		return true;
	}
	bool LDBVX_test() {
		PRINT_FUNC;
		do_op(0x60ff);
		do_op(0xa050);
		do_op(0xf033);
		if (memory[I] != 0x2 || memory[I + 1] != 5 || memory[I + 2] != 5) return false;
		return true;
	} 
	bool LDIVX_test() {
		PRINT_FUNC;
		do_op(0xa050);
		for (int i = 0; i < 15; i++) {
			do_op(0x60ff + i * 0x100);
		}
		do_op(0xfe55);
		for (int i = 0; i < 15; i++) {
			if (memory[I + i] != 0xff) return false;
		}
		return true;
	}
	bool LDVXI_test() {
		PRINT_FUNC;
		do_op(0xa050);
		for (int i = 0; i < 15; i++) {
			do_op(0x6000 + 0x100 * i);
		}
		for (int i = 80; i < 95; i++) {
			memory[i] = 0xf0;
		}
		do_op(0xfe65);
		for (int i = 0; i < 15; i++) {
			if (V[i] != 0xf0) return false;
		}
		return true;
	}
#endif