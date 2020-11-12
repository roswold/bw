#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <math.h>
#include <float.h>
#include <time.h>

#define TK_ESCAPE	SDL_SCANCODE_ESCAPE
#define TK_RIGHT	SDL_SCANCODE_RIGHT
#define TK_LEFT		SDL_SCANCODE_LEFT
#define TK_DOWN		SDL_SCANCODE_DOWN
#define TK_UP		SDL_SCANCODE_UP
#define TK_CONTROL	(SDL_SCANCODE_RCTRL|SDL_SCANCODE_LCTRL)
#define TK_SHIFT	(SDL_SCANCODE_RSHIFT|SDL_SCANCODE_LSHIFT)
#define TK_x		SDL_SCANCODE_X
#define TK_z		SDL_SCANCODE_Z
#define TK_a		SDL_SCANCODE_A
#define TK_c		SDL_SCANCODE_C

#define SND_ASYNC	0x100

#define aocoord(x,y,w) ((y)*(w)+(x)) //find array offset value corresponding to (x,y) coordinates in an array (given a width value)
#define aotox(ao,w) ((ao)%(w)) //find x value (given array offset and width value)
#define aotoy(ao,w) ((ao)/(w)) //find y value (given array offset and width value)

#define xor (!=)
#define frand(x) (((float)(rand()%1000)/1000.0f)*(float)(x))

#define MAPW 20
#define MAPH 15
#define MAPMAXTILES  300 //(MAPW*MAPH)20*15 tiles across 320x240 screen

#define MAPSCOLS 100 //how many columns of maps side-by-side? (imagine all possible maps exist like pixels in a bitmap format)

#define GRAVITY 0.2 //vertical speed compounded per frame until terminal velocity
#define TERMVEL 1.2 //terminal velocity
#define JUMPSPD 2.75

#define MAXHP 7
#define MAXANIM 10
#define MAXSHOTS 500 //we'll never need this many, right?!
#define PLSPD ((thirdbossdead)?(1.1):(1.0)) //this works
#define PLJSPD ((thirdbossdead)?(1.0):(0.7)) //jump/mid-air (horizontal) speed
#define PLDEF ((thirdbossdead)?(0.5):(1.0)) //defence upgrade (in the form of pre-processor defines because laziness)!

#define MAXENS 32 //max enemies


// #define EDITOR
// #define RECORDER

#define SOUND

//input recording---
#define TOTALRECORDINGS 4
#define RECORDLEN		1000 //how many frames of input recorded for cutscene
#define RECORDRNGVAL	10101010 //seed for recordings, for consistency
#define IR_L			0x001
#define IR_R			0x002
#define IR_U			0x004
#define IR_D			0x008
#define IR_Z			0x010
#define IR_ZD			0x020 //Z key pressed down this frame (as opposed to already down)
#define IR_X			0x040
#define IR_XD			0x080 //X key pressed down this frame (as opposed to already down)
#define IR_C			0x100


//make sure only one of these is true (!): ***maybe
int playing=true; //playing the game now
int title=true; //viewing the title screen
int dialogue=false; //reading dialogue
int drawing=true; //rendering game (!)
int gameover=false; //you died...
int playtheding=false;
int recording=false; //used for cutscene input recordings, eyyo!
int record_playback=false; //when true, recorded_input is used for player input
int quit_game=false;

int firstbossdead=false;
int secondbossdead=false;
int thirdbossdead=false;

int realthirdboss=0; //use this to determine how many 'twins' to kill!

char dialogue_text[512];
int dialogue_rendered;

int current_map=221;
int current_region=1; //different regions/areas of game
int story; //progression status
int shot_type=0;
int djump=false,can_djump=false; //ability to double jump + can double jump
int on_ground=false;
int can_jump=false;
int zdown=false; //z key held down
int lmdown=false; //left mouse button held down
int pl_dir; //facing
int pl_anim;

int cam_y=0; //for title intro rec
int title_xoff=0;

char *current_song_fname=NULL;
int recorded_input[RECORDLEN]; //store inputs frame-by-frame for replay
int current_frame=0; //should start when recording==true
int current_recording=0;


int cursx,cursy,curs_buttons,curs_last_pos=0; //for storing cursor x and y positions and mouse buttons
int edittype=2; //what to create

uint8_t kb_state[1024]={0};
uint8_t kb_state_old[1024]={0}; // store last frame's info for KeyDown vs KeyHeld


struct Map
{
	int tile[MAPMAXTILES];
};

void SetMusic();

#define mciSendString(cmd,file)
typedef union TPixel
{
	uint8_t a;
	uint8_t r;
	uint8_t g;
	uint8_t b;
} TPixel;
TPixel bg_color;

typedef struct Tigr
{
	uint32_t w,h;
	SDL_Window*win;
	SDL_Renderer*ren;
	SDL_Surface*sur;
	SDL_Texture*tex;
	TPixel*pix;
} Tigr;

void tigrFill(Tigr*scr,int x,int y,int w,int h,TPixel bgc)
{
	SDL_Rect rect={.x=x,.y=y,.w=w,.h=h};
	SDL_SetRenderDrawColor(scr->ren,bgc.r,bgc.g,bgc.b,bgc.a);
	SDL_RenderFillRect(scr->ren,&rect);
}

void tigrCheckKeys(void)
{
	static uint8_t*kb;

	SDL_PumpEvents();
	kb=(uint8_t*)SDL_GetKeyboardState(NULL);

	memcpy(kb_state_old,kb_state,1024);
	memcpy(kb_state,kb,1024);
}

void tigrUpdate(Tigr*scr)
{
	SDL_Event e;

	// Event loop
	if(SDL_PollEvent(&e))
	{
		switch(e.type)
		{
			case SDL_QUIT:
				playing=0;
				title=0;
				drawing=0;
				quit_game=1;
				break;
		}
	}

	SDL_RenderPresent(scr->ren);
}

void tigrFree(Tigr*scr)
{
	puts("quitting");
	if(scr->win)
		SDL_DestroyWindow(scr->win);
	//if(scr->ren)
		//SDL_DestroyRenderer(scr->ren);
	if(scr)
		free(scr);
	SDL_Quit();
}

Tigr*tigrLoadImage(char*fn,SDL_Renderer*r)
{
	FILE*f=fopen(fn,"r");
	Tigr*t=malloc(sizeof(Tigr));
	t->pix=malloc(sizeof(TPixel));
	if(!f)return NULL;

	t->sur=IMG_Load(fn);
	if(!t->sur)
	{
		free(t);
		return NULL;
	}

	t->tex=IMG_LoadTexture(r,fn);
	if(!t->tex)
	{
		free(t);
		return NULL;
	}

	t->w=t->sur->w;
	t->h=t->sur->h;

	// Load image
	return t;
}

Tigr*tigrWindow(uint32_t w,uint32_t h,char*title,uint32_t flags)
{
	Tigr*t=malloc(sizeof(Tigr));
	t->win=SDL_CreateWindow(title,
			SDL_WINDOWPOS_UNDEFINED,
			SDL_WINDOWPOS_UNDEFINED,
			w,h,0);
	t->w=w;
	t->h=h;
	t->ren=SDL_CreateRenderer(t->win,-1,0);

	//SDL_SetRenderDrawColor(t->ren,80,80,80,80);
	//SDL_RenderClear(t->ren);
	//SDL_RenderPresent(t->ren);
	return t;
}

int tigrClosed(Tigr*t)
{
	return !(playing||title);
}

uint8_t tigrKeyDown(Tigr*t,uint32_t k)
{
	// Return non-zero if key is depressed now
	uint8_t v=kb_state[k]&&(!kb_state_old[k]);
	if(v)
		kb_state_old[k]=1;
	return v;
}

uint8_t tigrKeyHeld(Tigr*t,uint32_t k)
{
	// Return non-zero if key is held
	return kb_state[k];
}

void PlaySound(char*fn,void*v,uint32_t flags)
{
	// Play sound
}

void tigrClear(Tigr*scr,TPixel col)
{
	SDL_SetRenderDrawColor(scr->ren,col.r,col.g,col.b,col.a);
	SDL_RenderClear(scr->ren);
}


void tigrBlitAlpha(Tigr*t,Tigr*spr,int x,int y,int w,int h,
		int a,int b,float alpha)
{
	if(!t||!spr) return;

	SDL_Rect src_rect={.x=0,.y=0,.w=t->w,.h=t->h};
	SDL_Rect des_rect={.x=x,.y=y,.w=spr->w,.h=spr->h};

	SDL_RenderCopy(t->ren,spr->tex,NULL,&des_rect);
}

void RandomMap(struct Map* map)
{
	int i;
	for(i=0;i<MAPMAXTILES;i++)
	{
		if(aotoy(i,MAPW)<5)
		{
			map->tile[i]=0;
			continue;
		}
		map->tile[i]=(rand()%3==0)?(0):(2); //set tile to on/off
	}
}

struct Ent //players, enemies..
{
	int exists;
	int hp,obeysgrav,type,hurt_time;
	int hdir,vdir; //horizontal direction (0=L,1=R) and vertical direction (0=U,1=D)
	float x,y,hsp,vsp;
};

void CreateEnt(struct Ent* ent,float x,float y,int type)
{
	ent->exists=true;
	ent->x=x;
	ent->y=y;
	ent->hsp=0.0;
	ent->vsp=0.0;
	ent->hdir=-1; //face left
	ent->hp=type+1; //default hp
	ent->type=type;
	ent->obeysgrav=true; //default behavior
	ent->hurt_time=0;
}

struct Shot //bullets, lazers...
{
	int exists;
	int type;
	float x,y,hsp,vsp;
	struct Ent *owner;
};

void CreateShot(struct Shot *shot,struct Ent *ent,int type) //create new shot from Ent data (or zero the memory if Ent* is NULL)
{
	if(ent==NULL)
	{
		shot->exists=false;
		return; //do not execute following code
	}
	shot->exists=true;
	shot->x=ent->x+8;
	shot->y=ent->y+8;
	shot->type=type;
	shot->owner=ent; //set owner ptr to Ent which created the shot

	float spd=1.8;
	switch(type)
	{
		// case 0: spd=1.5; break;
		case 1: spd=2.0; break;
		case 2: spd=2.2; break;
		case 3: spd=2.2; break;
		default: break;
	}
	shot->vsp=(float)(ent->vdir)*spd;
	shot->hsp=(ent->hdir==-1)?(-spd):(spd); //move in ent's hdir (default to right in case of 0 value)
}



struct Shot shots[MAXSHOTS]; //create here to allow for global functions and higher level stuffs
struct Ent en[MAXENS];



void Shoot(struct Ent *ent,int type)
{
	int i;
	for(i=0;i<MAXSHOTS;i++)
		if(!shots[i].exists)
		{
			CreateShot(&shots[i],ent,type);
			break;
		}
	//if all MAXSHOTS Shot structs 'exist,' then no shot will be created (!)
}

void ClearShots() //clear all shots (for new maps)
{
	int i;
	for(i=0;i<MAXSHOTS;i++)
		if(shots[i].exists) shots[i].exists=false;
}

typedef void (*MoveEnFn)(struct Ent*,struct Map *map,struct Ent*); //function type for moving enemies

//MoveEn[20] array of function pointers
MoveEnFn (*MoveEn[20])(struct Ent*,struct Map *map,struct Ent*); //create array of function pointers to call for different enemy types

//We still want to associate each MoveEn pointer with an actual function. Will be done in 'main' function

MoveEnFn MoveEnBasic(struct Ent* enem,struct Map *map,struct Ent* pl) //this is called for all enemy types for basic physics and consistency
{
	//apply friction and/or gravity
	enem->hsp*=0.75;
	if(fabs(enem->hsp)<0.1) enem->hsp=0;
	if(enem->obeysgrav)
		if(enem->vsp</*1.0-*/TERMVEL) enem->vsp+=GRAVITY;
	else
	{
		if(fabs(enem->vsp)<0.1) enem->vsp=0;
		enem->vsp*=0.75;
	}
	// enem->x+=enem->hsp;
	// enem->y+=enem->vsp;

	//collision detection for map tiles and applying velocity/speed
	if((enem->x+8>0 && enem->hsp<0) || ((enem->x+24)<(MAPW*16) && enem->hsp>0))
	{
		if(		( map->tile[aocoord((int)(roundf(((enem->x+2+((enem->hsp>0)?(enem->hsp):(enem->hsp)))/16))),(int)(roundf(((enem->y+2)/16))),MAPW)] <2 )
				&& ( map->tile[aocoord((int)(roundf(((enem->x+2+((enem->hsp>0)?(enem->hsp):(enem->hsp)))/16))),(int)(roundf(((enem->y+13)/16))),MAPW)] <2 )
				&& ( map->tile[aocoord((int)(roundf(((enem->x+14+((enem->hsp>0)?(enem->hsp):(enem->hsp)))/16))),(int)(roundf(((enem->y+2)/16))),MAPW)] <2 )
				&& ( map->tile[aocoord((int)(roundf(((enem->x+14+((enem->hsp>0)?(enem->hsp):(enem->hsp)))/16))),(int)(roundf(((enem->y+13)/16))),MAPW)] <2 ) )
		{
			enem->x+=enem->hsp;
		}
		else
		{
			enem->hsp*=0.8; //contact friction
		}
	}
	if((enem->y+6>0 && enem->vsp<0) || ((enem->y+24)<(MAPH*16) && enem->vsp>0))
	{
		if(		( map->tile[aocoord((int)(roundf(((enem->x+2)/16))), (int)(roundf(((enem->y+2+((enem->vsp>0)?(enem->vsp):(enem->vsp)))/16))),MAPW)] <2 )
				&& ( map->tile[aocoord((int)(roundf(((enem->x+14)/16))),(int)(roundf(((enem->y+2+((enem->vsp>0)?(enem->vsp):(enem->vsp)))/16))),MAPW)] <2 )
				&& ( map->tile[aocoord((int)(roundf(((enem->x+2)/16))), (int)(roundf(((enem->y+13+((enem->vsp>0)?(enem->vsp):(enem->vsp)))/16))),MAPW)] <2 )
				&& ( map->tile[aocoord((int)(roundf(((enem->x+14)/16))),(int)(roundf(((enem->y+13+((enem->vsp>0)?(enem->vsp):(enem->vsp)))/16))),MAPW)] <2 ) )
		//no collision in vertical axis (free-falling or mid-jump)
		{
			enem->y+=enem->vsp;
			// on_ground=false;
			// can_jump=false;
			// can_djump=false;
		}
		else //collision was detected in vertical axis
		{
			// if(enem->vsp<0.0) //collision was above player
				enem->vsp*=0.5; //contact friction
			/*else if(!can_jump) //collision was below player
			{
				can_jump=true;
				if(djump) can_djump=true;
				if(!on_ground)
				{
					if(enem->vsp>=TERMVEL) PlaySound("data/sfx/feet.wav",NULL,SND_ASYNC);
					on_ground=true;
				}
			}*/
		}
	}
}

MoveEnFn MoveEn1(struct Ent* enem,struct Map *map,struct Ent* pl) //eyeball
{
	if(fabs(enem->hsp)<0.01) //choose random new dir
	{
		enem->hsp=frand(2)-1.0; //between -1.0 and 1.0
		enem->vsp=frand(2)-1.0;
	}
	else //mutate current direction (with chance of following player)
	{
		if(rand()%10==0) //10% chance to change dir
		{
			enem->hsp=frand(0.1)-0.05; //between -1.0 and 1.0
			enem->vsp=frand(0.1)-0.05;
				if(rand()%5==0)
			{
				enem->hsp+=(pl->x-enem->x>0)?(0.02):(-0.02);
				enem->vsp+=(pl->y-enem->y>0)?(0.02):(-0.02);
			}
		}
	}
}

MoveEnFn MoveEn2(struct Ent* enem,struct Map *map,struct Ent* pl) //slug
{
	if(fabs(enem->hsp)<0.001 && rand()%20==0) //choose random new dir
	{
		enem->hsp=frand(6)-3.0;
		// enem->vsp=frand(4)-2.0;
	}
	else //mutate current direction (with chance of following player)
	{
		if(rand()%10==0)
		{
			enem->hsp=frand(0.5)-0.25; //between -1.0 and 1.0
			// enem->vsp=frand(0.5)-0.25;
		}
	}
}

MoveEnFn MoveEn3(struct Ent* enem,struct Map *map,struct Ent* pl) //guard
{
	if(enem->hsp!=0 || enem->vsp!=0) //doesn't move (except for gravity)
	{
		enem->hsp=0;
		// enem->vsp=0;
	}

	if(enem->hurt_time>0) enem->hurt_time--; //use this to time shooting
	else
	{
		enem->hurt_time=75;
		// enem->hdir=(pl->x-enem->x>0)?(1):(-1); //aim very goodfully!
		// enem->vdir=(pl->y-enem->y>0)?(1):(-1);
		// if(pl->x>enem->x) enem->hdir=1;
		// else enem->hdir=-1;
		// if(pl->y>enem->y) enem->vdir=1;
		// else enem->vdir=-1;
		enem->hdir=(rand()%2)?(1):(-1);
		enem->vdir=1-rand()%3;

		////printf("hd:%i,vd:%i\n",enem->hdir,enem->vdir);
		// printf("ex:%fpx:%fpp:%i\n",enem->x,pl->x,*pl);
		Shoot(enem,enem->type); //1:1 for enemy type and shot type
	}
}

MoveEnFn MoveEn4(struct Ent* enem,struct Map *map,struct Ent* pl) //eyeball boss
{
	if(fabs(enem->hsp)<0.001 && rand()%20==0) //choose random new dir
	{
		enem->hsp=frand(7)-3.5;
		enem->vsp=frand(7)-3.5;
	}
	else //mutate current direction (with chance of following player)
	{
		if(rand()%10==0)
		{
			enem->hsp=frand(0.5)-0.25; //between -1.0 and 1.0
			enem->vsp=frand(0.5)-0.25;
		}
	}

	if(enem->hurt_time>0) enem->hurt_time--; //use this to time shooting
	else
	{ //shoot in opposite directions at once
		if(rand()%10==0)
		{
			int afew=1+rand()%5;
			int i;
			for(i=0;i<MAXENS;i++)
			{
				if(!en[i].exists)
				{
					CreateEnt(&en[i],enem->x,enem->y,0); //create mini-eyeballs!!!
					en[i].obeysgrav=false;
					afew--;
				}
				if(afew<=0) break;
			}
		}
		enem->hurt_time=75;
		enem->hdir=(rand()%2)?(1):(-1);
		enem->vdir=1-rand()%3;
		Shoot(enem,1);
		//printf("hd:%i,vd:%i\n",enem->hdir,enem->vdir);
		enem->hdir*=-1;
		enem->vdir*=-1;
		Shoot(enem,1);
		//printf("hd:%i,vd:%i\n",enem->hdir,enem->vdir);

	}
}

MoveEnFn MoveEn5(struct Ent* enem,struct Map *map,struct Ent* pl) //slug boss
{
	if(fabs(enem->hsp)<0.001 && rand()%20==0) //choose random new dir
	{
		enem->hsp=frand(7)-3.5;
		// enem->vsp=frand(7)-3.5;
	}
	else //mutate current direction
	{
		if(rand()%10==0)
		{
			enem->hsp=frand(0.5)-0.25; //between -1.0 and 1.0
			enem->vsp=frand(0.2)-0.1; //FLLOOOAAATTING G SLSLUUUUGS@!!! YES, PLEASE. (Ahem, lost my cool there, sorry.)
		}
	}

	if(enem->hurt_time>0) enem->hurt_time--; //use this to time shooting
	else
	{ //shoot all directions
		if(rand()%10==0)
		{
			int afew=1+rand()%5;
			int i;
			for(i=0;i<MAXENS;i++)
			{
				if(!en[i].exists)
				{
					CreateEnt(&en[i],enem->x,enem->y,1); //create mini-slugs!
					en[i].obeysgrav=true;
					afew--;
				}
				if(afew<=0) break;
			}
		}
		enem->hurt_time=150;
		//up (and left/right)
		enem->hdir=(rand()%2)?(1):(-1);
		enem->vdir=-1;
		Shoot(enem,2);
		//down (and left/right)
		enem->hdir=(rand()%2)?(1):(-1);
		enem->vdir=1;
		Shoot(enem,2);
		//left
		enem->hdir=-1;
		enem->vdir=0;
		Shoot(enem,1);
		//right
		enem->hdir=1;
		enem->vdir=0;
		Shoot(enem,1);
		//printf("hd:%i,vd:%i\n",enem->hdir,enem->vdir);
		//printf("hd:%i,vd:%i\n",enem->hdir,enem->vdir);

	}
}

MoveEnFn MoveEn6(struct Ent* enem,struct Map *map,struct Ent* pl) //boox
{
	// enem->hsp=0;
	//Yes, that's right, it doesn't move.
}

MoveEnFn MoveEn7(struct Ent* enem,struct Map *map,struct Ent* pl) //swoop eye
{
	if(rand()%100==0 || (rand()%80==0 && fabs(pl->x-enem->x)<32)) //swoooooop
	{
		// enem->hsp=frand(2.5)-1.0;
		enem->vsp=frand(6)-3;
		if(fabs(enem->vsp)<0.01) enem->vsp*=-1;
	}
	else //mutate current direction
	{
		if(rand()%10==0)
		{
			// enem->hsp=frand(0.5)-0.25; //between -1.0 and 1.0
			enem->vsp*=1.5; //increase speed
		}
	}

	if(enem->hurt_time>0) enem->hurt_time--; //use this to time shooting
	else
	{
		//shoot downward
		if(rand()%60==0)
		{
			enem->hurt_time=75;
			enem->hdir=(pl->x-enem->x>0)?(1):(-1);
			enem->vdir=2;
			Shoot(enem,1);
			//printf("hd:%i,vd:%i\n",enem->hdir,enem->vdir);
		}
	}
}

MoveEnFn MoveEn8(struct Ent* enem,struct Map *map,struct Ent* pl) //swoop guard
{
	if(rand()%20==0 && fabs(pl->x-enem->x)<48 && fabs(pl->x-enem->x)<48) //ready to pounce
	{
		enem->hsp=(pl->x-enem->x>0)?(frand(0.9)):(frand(-0.9));
		// enem->vsp=frand(6)-3;
		// if(fabs(enem->vsp)<0.01) enem->vsp*=-1;
	}
	else //mutate current direction
	{
		if(rand()%100==0)
		{
			enem->hsp=frand(0.25)-0.125; //between -1.0 and 1.0
			// enem->vsp*=1.5; //increase speed
		}
	}

	if(enem->hurt_time>0) enem->hurt_time--; //use this to time shooting
	else
	{
		//shoot
		if(rand()%60==0)
		{
			enem->hurt_time=80;
			enem->hdir=(pl->x-enem->x>0)?(1):(-1); //aim very goodfully!
			if(rand()%10==0)
				enem->vdir=(pl->y-enem->y>0)?(1):(-1);
			else
				enem->vdir=0;
			// enem->hdir=(rand()%2)?(1):(-1);
			// enem->vdir=1-rand()%3;

			//printf("hd:%i,vd:%i\n",enem->hdir,enem->vdir);
			// printf("ex:%fpx:%fpp:%i\n",enem->x,pl->x,*pl);
			Shoot(enem,3); //1:1 for enemy type and shot type
		}
	}
}

MoveEnFn MoveEn9(struct Ent* enem,struct Map *map,struct Ent* pl) //twin eyes boss
{
	if(rand()%120==0 || (rand()%80==0 && fabs(pl->x-enem->x)<32)) //swooop
	{
		enem->hsp=frand(3)-1.5;
		enem->vsp=frand(6)-3;
		if(fabs(enem->vsp)<0.01) enem->vsp*=-1;
	}
	else //mutate current direction
	{
		if(rand()%10==0)
		{
			// enem->hsp=frand(0.5)-0.25; //between -1.0 and 1.0
			enem->vsp*=1.5; //increase speed
		}
	}

	if(enem->hurt_time>0) enem->hurt_time--; //use this to time shooting
	else
	{
		if(rand()%500==0)
		{
			int afew=1+rand()%5;
			int i;
			for(i=0;i<MAXENS;i++)
			{
				if(!en[i].exists)
				{
					CreateEnt(&en[i],enem->x+(16-rand()%32),enem->y,6); //create swoop eyes!
					en[i].obeysgrav=false;
					afew--;
				}
				if(afew<=0) break; //only create a few
			}
		}

		//shoot downward
		if(rand()%60==0)
		{
			enem->hurt_time=75;
			enem->hdir=(pl->x-enem->x>0)?(1):(-1);
			enem->vdir=2;
			Shoot(enem,1);
			//printf("hd:%i,vd:%i\n",enem->hdir,enem->vdir);
		}
	}
}


int SaveMap(struct Map *map,char *fname)
{
	FILE* f=fopen(fname,"wb");
	if(!f)
	{
		printf("map not saved\n");
		return 1; //file was not opened -- cancel
	}
	fwrite(map,sizeof(struct Map),1,f);
	fwrite(&bg_color,sizeof(uint32_t),1,f);
	fwrite(&current_region,sizeof(int),1,f);
	fwrite(en,sizeof(struct Ent),MAXENS,f);
	fclose(f);
	printf("map saved\n");
	return 0;
}

int OpenMap(struct Map *map,char *fname,int map_no)
{
	FILE* f=fopen(fname,"rb");
	if(!f)
	{
		printf("map not opened\n");
		return 1; //file was not opened -- cancel
	}
	fread(map,sizeof(struct Map),1,f);
	fread(&bg_color,sizeof(uint32_t),1,f);
	fread(&current_region,sizeof(int),1,f);
	fread(en,sizeof(struct Ent),MAXENS,f);

	current_map=map_no;

	//check for undead bosses, then remove them
	int i;
	for(i=0;i<MAXENS;i++)
	{
		if(en[i].exists && en[i].type==3 && ( (firstbossdead!=0 && current_map==204) || (firstbossdead==2 && current_map==115) ) )
			en[i].exists=false;
		if(en[i].exists && en[i].type==4 && ( (secondbossdead!=0 && current_map==403) || (secondbossdead==2 && current_map==117) ) )
			en[i].exists=false;
		if(en[i].exists && en[i].type==8 && ( (current_map==8 && thirdbossdead!=0) || (current_map==217 && thirdbossdead==2) ) )
			en[i].exists=false;
	}

	if(firstbossdead==2 && current_map==115)
	{
		map->tile[aocoord(19,8,MAPW)]=1;
		map->tile[aocoord(19,9,MAPW)]=1; //open the door after boss is dead!
		current_region=5;
	}
	if(secondbossdead==2 && current_map==117)
	{
		map->tile[aocoord(19,4,MAPW)]=1;
		map->tile[aocoord(19,5,MAPW)]=1; //open the door after boss is dead!
		current_region=5;
	}
	if(thirdbossdead==2 && current_map==217)
	{
		map->tile[aocoord(0,5,MAPW)]=1;
		map->tile[aocoord(0,6,MAPW)]=1; //open the door after boss is dead!
		current_region=5;
	}
	if(thirdbossdead>0 && current_map==8)
	{
		map->tile[aocoord(19,6,MAPW)]=1;
		map->tile[aocoord(18,6,MAPW)]=0; //open the door after boss is dead!
	}

	fclose(f);
	//printf("map opened\n");
	return 0;
}

void Reset(struct Ent *pl,struct Map *map)
{
	CreateEnt(pl,16,48,0);
	pl->hp=MAXHP;
	pl->exists=true;
	pl->hsp=0.0;
	pl->vsp=0.0;
	pl_dir=0;
	// gameover=false;
	// playing=true;
	// title=false;
	// dialogue=false;
	firstbossdead=0;
	secondbossdead=0;
	thirdbossdead=0;
	current_map=1;
	current_region=1;
	shot_type=0;
	djump=false;
	can_djump=false;
	on_ground=false;
	can_jump=false;
	ClearShots();

	OpenMap(map,"data/maps/map1.dat",1);
	if(title) return;

	#ifdef SOUND
	if(!title)
	{
		SetMusic();
	}

	if( (current_map==204 && firstbossdead!=0) || (current_map==115 && firstbossdead==2) ) mciSendString("stop","data/sfx/beatem.mp3");
	if( (current_map==403 && secondbossdead!=0) || (current_map==117 && secondbossdead==2) ) mciSendString("stop","data/sfx/beatem.mp3");
	if( (current_map==8 && thirdbossdead!=0) || (current_map==217 && thirdbossdead==2) ) mciSendString("stop","data/sfx/beatem.mp3");
	#endif //SOUND
	printf("reset complete\n");
}

void SaveGame(struct Ent *pl,char *fname)
{
	FILE* f=fopen(fname,"wb");
	if(!f) return; //file was not opened -- cancel
	fwrite(&firstbossdead,sizeof(int),1,f);
	fwrite(&secondbossdead,sizeof(int),1,f);
	fwrite(&current_map,sizeof(int),1,f);
	fwrite(&pl->x,sizeof(float),1,f);
	fwrite(&pl->y,sizeof(float),1,f);
	fwrite(&thirdbossdead,sizeof(int),1,f);
	fclose(f);
	printf("%s save complete\n",fname);
}

int LoadGame(struct Ent *pl,struct Map *map,char *fname)
{
	FILE* f=fopen(fname,"rb");
	if(!f) return 1; //file was not opened -- cancel
	fread(&firstbossdead,sizeof(int),1,f);
	fread(&secondbossdead,sizeof(int),1,f);
	fread(&current_map,sizeof(int),1,f);
	fread(&pl->x,sizeof(float),1,f);
	fread(&pl->y,sizeof(float),1,f);
	fread(&thirdbossdead,sizeof(int),1,f);
	fclose(f);

	char fn[512];
	sprintf(fn,"data/maps/map%i.dat",current_map);
	OpenMap(map,fn,current_map);
	if(firstbossdead) djump=true; else djump=false;
	if(secondbossdead) shot_type=1; else shot_type=0;
	// if(thirdbossdead) shot_type=2;
	pl->hp=MAXHP;

	printf("%s load complete\n",fname);
	return 0;
}

int SaveRec(char *fname)
{
	FILE* f=fopen(fname,"wb");
	if(!f) return 1; //file was not opened -- cancel

	fwrite(recorded_input,sizeof(int),RECORDLEN,f);

	fclose(f);
	return 0;
}

int LoadRec(char *fname)
{
	FILE* f=fopen(fname,"rb");
	if(!f) return 1; //file was not opened -- cancel

	fread(recorded_input,sizeof(int),RECORDLEN,f);

	fclose(f);
	return 0;
}

void DrawSineTitle(Tigr *scr, Tigr *spr,int xpos,int ypos)
{
	int x,y;
	static float find_sin,sin_n;
	// sin_q=0.0001; //'LFO freq'
	for(x=0;x<spr->w;x++)
	{
		sin_n+=0.75;
		if(sin_n>=FLT_MAX) sin_n=0.0; //float.h

		for(y=0;y<spr->h;y++)
		{
			if(spr->pix[aocoord(x,y,spr->w)].a==0) continue; //skip pixels with alpha of 0
			if(xpos+x>0 && ypos+y>0 && xpos+x<320 && ypos+y<240) continue; //skip pixels outside screen
			find_sin=( sin(sin_n*16)*2.0 );
			scr->pix[aocoord(xpos+x,ypos+y+(int)(find_sin),scr->w)]=spr->pix[aocoord(x,y,spr->w)];
		}
	}
}

void SetMusic()
{
	switch(current_region)
	{
		case 2: mciSendString("stop","data/sfx/enthusra.mp3"); //area 2
				mciSendString("stop","data/sfx/upsanddowns.mp3");
				mciSendString("stop","data/sfx/beatem.mp3");
				mciSendString("stop","data/sfx/scatterbrain.mp3");
				mciSendString("play","data/sfx/forward.mp3");
				current_song_fname="data/sfx/forward.mp3"; break;
		case 3: mciSendString("stop","data/sfx/enthusra.mp3"); //area 3
				mciSendString("stop","data/sfx/upsanddowns.mp3");
				mciSendString("stop","data/sfx/forward.mp3");
				mciSendString("stop","data/sfx/scatterbrain.mp3");
				mciSendString("play","data/sfx/beatem.mp3");
				current_song_fname="data/sfx/beatem.mp3"; break;
		case 4: mciSendString("stop","data/sfx/forward.mp3"); //area 4
				mciSendString("stop","data/sfx/upsanddowns.mp3");
				mciSendString("stop","data/sfx/beatem.mp3");
				mciSendString("stop","data/sfx/enthusra.mp3");
				mciSendString("play","data/sfx/scatterbrain.mp3");
				current_song_fname="data/sfx/scatterbrain.mp3"; break;
		case 5: mciSendString("stop","data/sfx/forward.mp3"); //area 5
				mciSendString("stop","data/sfx/scatterbrain.mp3");
				mciSendString("stop","data/sfx/beatem.mp3");
				mciSendString("stop","data/sfx/enthusra.mp3");
				mciSendString("play","data/sfx/upsanddowns.mp3");
				current_song_fname="data/sfx/upsanddowns.mp3"; break;
		default: mciSendString("stop","data/sfx/forward.mp3"); //area 1
				mciSendString("stop","data/sfx/upsanddowns.mp3");
				mciSendString("stop","data/sfx/beatem.mp3");
				mciSendString("stop","data/sfx/scatterbrain.mp3");
				mciSendString("play","data/sfx/enthusra.mp3");
				current_song_fname="data/sfx/enthusra.mp3"; break;
	}
}


int main(int argc, char **argv)
{
	srand(time(NULL));

	struct Map map;
	// bg_color=tigrRGB(0x00,0x00,0x00);
	// RandomMap(&map);

	#ifdef EDITOR
	#endif //EDITOR

	struct Ent pl;
	CreateEnt(&pl,16,48,0);
	pl.hp=MAXHP;

	current_recording=rand()%TOTALRECORDINGS; //go to random recording for intro!

	//which enemies use which movement functions:
	MoveEn[0]=(*MoveEn1);
	MoveEn[1]=(*MoveEn2);
	MoveEn[2]=(*MoveEn3);
	MoveEn[3]=(*MoveEn4);
	MoveEn[4]=(*MoveEn5);
	MoveEn[5]=(*MoveEn6);
	MoveEn[6]=(*MoveEn7);
	MoveEn[7]=(*MoveEn8);
	MoveEn[8]=(*MoveEn9);

	SDL_Init(SDL_INIT_EVERYTHING);

	#ifndef EDITOR
	// Reset(&pl,&map);
	// LoadGame(&pl,&map,"data/profile.dat");
	#endif //EDITOR

	#ifdef SOUND
	//open all music files
	mciSendString("open","data/sfx/forward.mp3");
	mciSendString("open","data/sfx/enthusra.mp3");
	// mciSendString("open","data/sfx/beatem.mp3");
	// mciSendString("open","data/sfx/upsanddowns.mp3");
	mciSendString("open","data/sfx/scatterbrain.mp3");

	if(title)
	{
		mciSendString("play","data/sfx/upsanddowns.mp3");
		current_song_fname="data/sfx/upsanddowns.mp3";
	}
	else
	{
		SetMusic();
		if( (current_map==204 && firstbossdead!=0) || (current_map==115 && firstbossdead==2) ) mciSendString("stop","data/sfx/beatem.mp3");
		if( (current_map==403 && secondbossdead!=0) || (current_map==117 && secondbossdead==2) ) mciSendString("stop","data/sfx/beatem.mp3");
		if( (current_map==8 && thirdbossdead!=0) || (current_map==217 && thirdbossdead==2) ) mciSendString("stop","data/sfx/beatem.mp3");
	}
	#endif //SOUND

	Tigr *spr_tile[6]={0}; //array of sprite pointers to load
	Tigr *spr_shot[4]={0};
	Tigr *spr_en[20]={0}; //enemy sprites
	Tigr *spr_pl[4]={0};
	Tigr *spr_hp[2]={0};

	Tigr *screen = tigrWindow(320, 240, "ld48hr40 The Beforlorne Wander", 0);
	spr_pl[0]=tigrLoadImage("data/gfx/plL.png",screen->ren);
	spr_pl[1]=tigrLoadImage("data/gfx/plL2.png",screen->ren);
	spr_pl[2]=tigrLoadImage("data/gfx/plR.png",screen->ren);
	spr_pl[3]=tigrLoadImage("data/gfx/plR2.png",screen->ren);
	spr_shot[0]=tigrLoadImage("data/gfx/shot1.png",screen->ren);
	spr_shot[1]=tigrLoadImage("data/gfx/shot2.png",screen->ren);
	spr_shot[2]=tigrLoadImage("data/gfx/shot3.png",screen->ren);
	spr_shot[3]=tigrLoadImage("data/gfx/shot4.png",screen->ren);
	spr_tile[0]=tigrLoadImage("data/gfx/tile0.png",screen->ren); //does not correspond with tile value 0, is only for editi,screen->renng
	spr_tile[1]=tigrLoadImage("data/gfx/tile1.png",screen->ren);
	spr_tile[2]=tigrLoadImage("data/gfx/tile2.png",screen->ren);
	spr_tile[3]=tigrLoadImage("data/gfx/tile3.png",screen->ren);
	spr_tile[4]=tigrLoadImage("data/gfx/tile4.png",screen->ren);
	spr_tile[5]=tigrLoadImage("data/gfx/tile5.png",screen->ren);
	spr_en[0]=tigrLoadImage("data/gfx/en1.png",screen->ren);
	spr_en[1]=tigrLoadImage("data/gfx/en2.png",screen->ren);
	spr_en[2]=tigrLoadImage("data/gfx/en3.png",screen->ren);
	spr_en[3]=tigrLoadImage("data/gfx/en4.png",screen->ren);
	spr_en[4]=tigrLoadImage("data/gfx/en5.png",screen->ren);
	spr_en[5]=tigrLoadImage("data/gfx/en6.png",screen->ren);
	spr_en[6]=tigrLoadImage("data/gfx/en7.png",screen->ren);
	spr_en[7]=tigrLoadImage("data/gfx/en8.png",screen->ren);
	spr_en[8]=tigrLoadImage("data/gfx/en9.png",screen->ren);
	spr_hp[0]=tigrLoadImage("data/gfx/hp.png",screen->ren);
	spr_hp[1]=tigrLoadImage("data/gfx/nohp.png",screen->ren);
	Tigr *spr_tb=tigrLoadImage("data/gfx/txtbox.png",screen->ren);
	Tigr *spr_title=tigrLoadImage("data/gfx/title.png",screen->ren);

	tigrClear(screen,(TPixel){0});
	tigrUpdate(screen);

	//start intro playback
	{
		char str[128];
		sprintf(str,"data/rec/p%i.dat",current_recording);
		LoadGame(&pl,&map,str);
		sprintf(str,"data/rec/rec%i.dat",current_recording);
		LoadRec(str);

		current_frame=0;
		recording=false;
		record_playback=true;
		srand(RECORDRNGVAL);
	}


	while (!tigrClosed(screen))
	{
		//tigrClear(screen,(TPixel){0});
		tigrCheckKeys();

		if(tigrKeyDown(screen,TK_ESCAPE)) //go back to title
		{
			puts("pressed ESCAPE");
			if(!title)
			{
				// exit(0);
				title=true;
				playing=true;
				drawing=true;

				char str[128];
				sprintf(str,"data/rec/p%i.dat",current_recording);
				LoadGame(&pl,&map,str);
				sprintf(str,"data/rec/rec%i.dat",current_recording);
				LoadRec(str);

				current_frame=0;
				recording=false;
				record_playback=true;
				srand(RECORDRNGVAL);

				//stop everything and play title song
				mciSendString("stop","data/sfx/enthusra.mp3");
				mciSendString("stop","data/sfx/beatem.mp3");
				mciSendString("stop","data/sfx/forward.mp3");
				mciSendString("stop","data/sfx/scatterbrain.mp3");

				mciSendString("play","data/sfx/upsanddowns.mp3");
				current_song_fname="data/sfx/upsanddowns.mp3";
			}
			else //exit game
			{
				exit(0);
			}
		}


		//title screen---
		if(title)
		{
			//input (title)-----
			if(tigrKeyDown(screen,TK_z)) //new game
			{
				recording=false;
				record_playback=false;
				dialogue=false;
				playing=true;
				title=false;
				drawing=true;
				Reset(&pl,&map);
			}

			if(tigrKeyDown(screen,TK_c)) //continue
			{
				int success;
				success=!LoadGame(&pl,&map,"data/profile.dat");
				if(success)
				{
					dialogue=false;
					playing=true;
					title=false;
					drawing=true;
					ClearShots();
					recording=false;
					record_playback=false;
				}
			}

			if(playing && !title)
			{
				#ifdef SOUND
				SetMusic();
				if( (current_map==204 && firstbossdead!=0) || (current_map==115 && firstbossdead==2) ) mciSendString("stop","data/sfx/beatem.mp3");
				if( (current_map==403 && secondbossdead!=0) || (current_map==117 && secondbossdead==2) ) mciSendString("stop","data/sfx/beatem.mp3");
				if( (current_map==8 && thirdbossdead!=0) || (current_map==217 && thirdbossdead==2) ) mciSendString("stop","data/sfx/beatem.mp3");
				#endif //SOUND
			}

			//render (title)-----
			// tigrClear(screen, bg_color);
			//SDL_RenderClear(screen);

			// tigrPrint(screen, tfont, 140, 120, tigrRGB(0xff, 0xff, 0xff), "The Beforlorne Wander\nZ to Start New\nC to Continue Last");
			// tigrPrint(screen, tfont, 250, 220, tigrRGB(0xff, 0xff, 0xcc), "pvtroswold");

			// tigrUpdate(screen); //done in tigr window while loop



		}

		if(playing) //playing the game
		{
			//input-----
			// if(tigrKeyDown(screen,SDL_SCANCODE_R)) Reset(&pl,&map);
			// if(tigrKeyDown(screen,SDL_SCANCODE_E)) LoadGame(&pl,&map,"data/profile.dat");
			// if(tigrKeyDown(screen,SDL_SCANCODE_W)) OpenMap(&map,"data/maps/map200.dat",200);
			if(pl.exists)
			{
				if(tigrKeyDown(screen,TK_a) && !recording && !record_playback && !title)
				{
					// PlaySound("data/sfx/yaysound.wav",NULL,SND_ASYNC);
					#ifdef SOUND
					char str[512];
					sprintf(str,"pause %s",current_song_fname);
					mciSendString(str,"file");
					#endif //SOUND
					sprintf(dialogue_text,"press C to continue");
					playing=false;
					drawing=false;
					dialogue=true;
					dialogue_rendered=false;
				}
				if( (!record_playback && !title && tigrKeyHeld(screen,TK_RIGHT) && pl.hsp<0.8) || (record_playback && recorded_input[current_frame]&IR_R) ) //!= is logical XOR
				{
					pl_anim=(pl_anim+1)%MAXANIM;
					pl.hsp+=(on_ground)?(1.0*PLSPD):(PLJSPD*PLSPD); //more traction on ground so greater speed
					pl.hdir=1;
					pl_dir=1;
					// printf("R:%x\n",IR_R);
					if(recording) recorded_input[current_frame]|=IR_R;
				}
				if( ( (!record_playback && !title && tigrKeyHeld(screen,TK_LEFT)) || (record_playback && recorded_input[current_frame]&IR_L) ) && pl.hsp>-0.8)
				{
					pl_anim=(pl_anim+1)%MAXANIM;
					pl.hsp-=(on_ground)?(1.0*PLSPD):(PLJSPD*PLSPD);
					pl.hdir=-1;
					pl_dir=0;
					// printf("L:%x\n",IR_L);
					if(recording) recorded_input[current_frame]|=IR_L;
				}
				// pl.hdir=0;
				if( (!record_playback && !title && tigrKeyHeld(screen,TK_DOWN)) || (record_playback && recorded_input[current_frame]&IR_D) )
				{
					pl.vdir=1;
					// printf("D:%x\n",IR_D);
					if(recording) recorded_input[current_frame]|=IR_D;
				}
				else if( (!record_playback && !title && tigrKeyHeld(screen,TK_UP)) || (record_playback && recorded_input[current_frame]&IR_U) )
				{
					pl.vdir=-1;
					// printf("U:%x\n",IR_U);
					if(recording) recorded_input[current_frame]|=IR_U;
				}
				else pl.vdir=0; //vdir is zero unless UP/DOWN is (are) held

				if( (!record_playback && !title && tigrKeyDown(screen,TK_z)) || (record_playback && recorded_input[current_frame]&IR_ZD)) //jump button
				{
					if((can_jump || can_djump) /*&& !zdown*/)
					{
						if(!can_jump && can_djump) can_djump=false; //used double jump
						pl.vsp=-JUMPSPD; //jump
						zdown=true;
					}
					// printf("TK_z:%x\n",IR_ZD);
					if(recording) recorded_input[current_frame]|=IR_ZD;
				}
				else if(zdown && on_ground) zdown=false;

				// if(!tigrKeyDown(screen,TK_z) && tigrKeyHeld(screen,TK_z)) //key down, but not starting this frame
					// if(recording) recorded_input[current_frame]|=IR_Z;

				if( (!record_playback && !title && tigrKeyDown(screen,TK_x)) || (record_playback && recorded_input[current_frame]&IR_XD) ) //shoot bullet
				{
					Shoot(&pl,shot_type);
					PlaySound("data/sfx/shoot.wav",NULL,SND_ASYNC);
					// printf("TK_x:%x\n",IR_XD);
					if(recording) recorded_input[current_frame]|=IR_XD;
				}
				else if(!title && tigrKeyHeld(screen,TK_x))
					if(recording) recorded_input[current_frame]|=IR_X;

				// pl.hsp*=(on_ground)?(0.75):(0.76); //less horizontal friction if airborne
				pl.hsp*=0.75;
				if(fabs(pl.hsp)<0.1) pl.hsp=0;
				if(pl.vsp</*1.0-*/TERMVEL) pl.vsp+=GRAVITY;
				// pl.x+=pl.hsp;
				// pl.y+=pl.vsp;
			}


			//manage cam_y
			if(title) //(title && playing)==true means we are playing intro recordings, so use the camera y
			{
				cam_y=-pl.y+60;
			}
			else if(cam_y!=0)
			{
				cam_y=0;
			}

			//recorder options/input
			#ifdef RECORDER
				//recording stuff
				if(tigrKeyDown(screen,SDL_SCANCODE_B))
				{
					// SaveGame(&pl,"data/rec/pX.dat");
					SaveRec("data/rec/recX.dat");
				}
				if(tigrKeyDown(screen,SDL_SCANCODE_N))
				{
					LoadGame(&pl,&map,"data/rec/pX.dat");
					LoadRec("data/rec/recX.dat");
				}
				if(tigrKeyDown(screen,SDL_SCANCODE_G))
				{
					if(!recording)
					{
						SaveGame(&pl,"data/rec/pX.dat");
						//new recording, clear previous
						int i;
						for(i=0;i<RECORDLEN;i++) recorded_input[i]=0;
						record_playback=false;
						current_frame=0;
						recording=true;
						srand(RECORDRNGVAL);
					}
					else
						//stop recording
						recording=false;
				}
				if(tigrKeyDown(screen,SDL_SCANCODE_H))
				{
					recording=false;
					current_frame=0;
					record_playback=!record_playback; //toggle playback on/off, start from beginning
					srand(RECORDRNGVAL);
				}
			#endif //RECORDER

			#ifdef EDITOR
			if(!title) //don't edit in the title/intro
			{
				tigrMouse(screen,&cursx,&cursy,&curs_buttons);
				int curs_new_pos=aocoord((cursx/16),(cursy/16),MAPW);
				if( (curs_buttons & VK_LBUTTON) && (!lmdown || curs_new_pos!=curs_last_pos)) //left mouse button - click to create tile/enemy
				{
					if(edittype<100)
						map.tile[curs_new_pos]=(map.tile[curs_new_pos]!=edittype)?(edittype):(0);
					else
					{
						//100 series of edittype: make an enemy
						int i;
						for(i=0;i<MAXENS;i++)
						{
							if(!en[i].exists)
							{
								CreateEnt(&en[i],(float)((int)(cursx/16)*16)-6,(float)((int)(cursy/16)*16)-8,(edittype-100));
								switch(edittype-100)
								{
									case 0: //eyeball
										en[i].obeysgrav=false;
										break;
									case 1: //slug
										en[i].obeysgrav=true;
										break;
									case 2: //guard
										en[i].hurt_time=75; //used for shot timing
										en[i].obeysgrav=true;
										break;
									case 3: //eyeball boss
										en[i].hp=60;
										en[i].hurt_time=75; //used for shot timing
										en[i].obeysgrav=false;
										break;
									case 4: //slug boss
										en[i].hp=70;
										en[i].hurt_time=150; //used for shot timing
										en[i].obeysgrav=false;
										break;
									case 5: //boox
										en[i].hp=1;
										en[i].obeysgrav=true;
										break;
									case 6: //swoop eye
										en[i].hp=10;
										en[i].hurt_time=75; //used for shot timing
										en[i].obeysgrav=false;
										en[i].x--;
										break;
									case 7: //swoop guard
										en[i].hp=14;
										en[i].hurt_time=80; //used for shot timing
										en[i].obeysgrav=true;
										en[i].y+=2;
										break;
									case 8: //twin eyes
										en[i].hp=130;
										en[i].hurt_time=100; //used for shot timing
										en[i].obeysgrav=false;
										break;
									default: break;
								}
								printf("en created (t:%i)\n",(edittype-100));
								break; //only create one
							}
						}
					}
					lmdown=true;
				}
				else if( !(curs_buttons & VK_LBUTTON) ) //left mouse button not down
				{
					lmdown=false;
				}
				if(tigrKeyHeld(screen,TK_CONTROL))
				{
					if(tigrKeyDown(screen,SDL_SCANCODE_S))
					{
						char fname[512];
						sprintf(fname,"data/maps/map%i.dat",current_map);
						SaveMap(&map,fname);
					}
					if(tigrKeyDown(screen,SDL_SCANCODE_O))
					{
						char fname[512];
						sprintf(fname,"data/maps/map%i.dat",current_map);
						//remove enemies (before loading map
						int i;
						for(i=0;i<MAXENS;i++)
							en[i].exists=false;
						OpenMap(&map,fname,current_map);
					}
				}
				//edit tiles
				if(tigrKeyDown(screen,'1')) edittype=1;
				if(tigrKeyDown(screen,'2')) edittype=2;
				if(tigrKeyDown(screen,'3')) edittype=3;
				if(tigrKeyDown(screen,'4')) edittype=4;
				if(tigrKeyDown(screen,'5')) edittype=5;
				// if(tigrKeyDown(screen,'4')) edittype=4;

				//edit enemies
				if(tigrKeyHeld(screen,TK_CONTROL))
				{
					if(tigrKeyDown(screen,'1')) edittype=100; //100 series of edittype is reserved for enemies
					if(tigrKeyDown(screen,'2')) edittype=101;
					if(tigrKeyDown(screen,'3')) edittype=102;
					if(tigrKeyDown(screen,'4')) edittype=103;
					if(tigrKeyDown(screen,'5')) edittype=104;
					if(tigrKeyDown(screen,'6')) edittype=105; //booox
					if(tigrKeyDown(screen,'7')) edittype=106; //swoop eye
					if(tigrKeyDown(screen,'8')) edittype=107; //swoop guard
					if(tigrKeyDown(screen,'9')) edittype=108; //twin eyes
				}

				//SHIFT+Q: delete all enemies at once
				if(tigrKeyDown(screen,SDL_SCANCODE_Q) && tigrKeyHeld(screen,TK_SHIFT))
				{
					int i;
					for(i=0;i<MAXENS;i++)
						en[i].exists=false;
					printf("deleted all enemies\n");
				}

				if(tigrKeyDown(screen,SDL_SCANCODE_U)) bg_color=tigrRGB((char)(rand()%0x33),(char)(rand()%0x33),(char)(rand()%0x33)); //random dark color
				if(tigrKeyDown(screen,SDL_SCANCODE_K)) bg_color=tigrRGB((char)(0x33+(rand()%0xff)),(char)(0x33+(rand()%0xff)),(char)(0x33+(rand()%0xff))); //random light color
				if(tigrKeyDown(screen,SDL_SCANCODE_J)) bg_color=tigrRGB(0x00,0x00,0x00);
				if(tigrKeyDown(screen,SDL_SCANCODE_T)) current_region--;
				if(tigrKeyDown(screen,SDL_SCANCODE_Y)) current_region++;


				curs_last_pos=curs_new_pos; //last array offset index value for cursor on screen
			}
			#endif //EDITOR



			//manage player
			if(pl.exists)
			{
				if(map.tile[aocoord((int)((pl.x+16)/16),(int)((pl.y+16)/16),MAPW)] == 1) //portal/door
				{
					int new_map=current_map; //default value for new map to load
					// pl.exists=false;
					if((int)((pl.x+16)/16)==0) //horizontal doors
					{
						pl.x=(float)(MAPW-2)*16.0;
						new_map=aocoord(aotox(current_map,MAPSCOLS)-1,aotoy(current_map,MAPSCOLS),MAPSCOLS);
					}
					else if((int)((pl.x+16)/16)==MAPW-1)
					{
						pl.x=1;
						new_map=aocoord(aotox(current_map,MAPSCOLS)+1,aotoy(current_map,MAPSCOLS),MAPSCOLS);
					}
					else if((int)((pl.y+16)/16)==0) //vertical doors
					{
						pl.y=(float)(MAPH-2)*16.0;
						new_map=aocoord(aotox(current_map,MAPSCOLS),aotoy(current_map,MAPSCOLS)-1,MAPSCOLS);
					}
					else if((int)((pl.y+16)/16)==MAPH-1)
					{
						pl.y=1;
						new_map=aocoord(aotox(current_map,MAPSCOLS),aotoy(current_map,MAPSCOLS)+1,MAPSCOLS);
					}
					current_map=new_map;
					char fname[512]; //store name for loading map
					sprintf(fname,"data/maps/map%i.dat",new_map);
					int old_region=current_region;

					//remove enemies (before loading map
					/*{
						int i;
						for(i=0;i<MAXENS;i++)
							en[i].exists=false;
					}*/

					OpenMap(&map,fname,current_map);

					//change the music
					#ifdef SOUND
					if(old_region!=current_region && !title)
					{
						SetMusic();
					}

					if( (current_map==204 && firstbossdead!=0) || (current_map==115 && firstbossdead==2) ) mciSendString("stop","data/sfx/beatem.mp3");
					if( (current_map==403 && secondbossdead!=0) || (current_map==117 && secondbossdead==2) ) mciSendString("stop","data/sfx/beatem.mp3");
					if( (current_map==8 && thirdbossdead!=0) || (current_map==217 && thirdbossdead==2) ) mciSendString("stop","data/sfx/beatem.mp3");
					if( (current_map==8 && thirdbossdead==0) || (current_map==217 && thirdbossdead<2) )
						realthirdboss=2; //have to kill both 'twins'

					#endif //SOUND

					ClearShots();
				}
			}

			if(pl.exists)
			{
				//collision detection is C+P from LD39 because I didn't want to spend the entirety of DAY 1 on collision detection alone.
				if((pl.x+8>0 && pl.hsp<0) || ((pl.x+24)<(MAPW*16) && pl.hsp>0))
				{
					if(		( (map.tile[aocoord((int)(roundf(((pl.x+2+((pl.hsp>0)?(pl.hsp):(pl.hsp)))/16))),(int)(roundf(((pl.y+2)/16))),MAPW)]) <2 )
							&& ( (map.tile[aocoord((int)(roundf(((pl.x+2+((pl.hsp>0)?(pl.hsp):(pl.hsp)))/16))),(int)(roundf(((pl.y+14)/16))),MAPW)]) <2 )
							&& ( (map.tile[aocoord((int)(roundf(((pl.x+14+((pl.hsp>0)?(pl.hsp):(pl.hsp)))/16))),(int)(roundf(((pl.y+2)/16))),MAPW)]) <2 )
							&& ( (map.tile[aocoord((int)(roundf(((pl.x+14+((pl.hsp>0)?(pl.hsp):(pl.hsp)))/16))),(int)(roundf(((pl.y+14)/16))),MAPW)]) <2 ) )
					{
						pl.x+=pl.hsp;
					}
					else
					{
						pl.hsp*=0.8; //contact friction
					}
				}
				if((pl.y+6>0 && pl.vsp<0) || ((pl.y+24)<(MAPH*16) && pl.vsp>0))
				{
					if(		( (map.tile[aocoord((int)(roundf(((pl.x+2)/16))), (int)(roundf(((pl.y+2+((pl.vsp>0)?(pl.vsp):(pl.vsp)))/16))),MAPW)]) <2 )
							&& ( (map.tile[aocoord((int)(roundf(((pl.x+14)/16))),(int)(roundf(((pl.y+2+((pl.vsp>0)?(pl.vsp):(pl.vsp)))/16))),MAPW)]) <2 )
							&& ( (map.tile[aocoord((int)(roundf(((pl.x+2)/16))), (int)(roundf(((pl.y+14+((pl.vsp>0)?(pl.vsp):(pl.vsp)))/16))),MAPW)]) <2 )
							&& ( (map.tile[aocoord((int)(roundf(((pl.x+14)/16))),(int)(roundf(((pl.y+14+((pl.vsp>0)?(pl.vsp):(pl.vsp)))/16))),MAPW)]) <2 ) )
					//no collision in vertical axis (free-falling or mid-jump)
					{
						pl.y+=pl.vsp;
						on_ground=false;
						can_jump=false;
						// can_djump=false;
					}
					else //collision was detected in vertical axis
					{
						if(pl.vsp<0.0) //collision was above player
							pl.vsp*=0.5; //contact friction
						else if(!can_jump) //collision was below player
						{
							can_jump=true;
							if(djump) can_djump=true;
							if(!on_ground)
							{
								if(pl.vsp>=TERMVEL) PlaySound("data/sfx/feet.wav",NULL,SND_ASYNC);
								on_ground=true;
							}
						}
					}
				}
				if(pl.hurt_time>0) pl.hurt_time--;
			}

			if(pl.hsp==0.0 && pl_anim>0) pl_anim=0; //stop walking when still

			//GAME OVER ***
			if((pl.hp<=0 || !pl.exists) && !title) //D-E-D
			{
				sprintf(dialogue_text,"GAME OVER\nTry Again?\n Press C to continue");

				pl.exists=false;

				#ifdef SOUND
				char str[512];
				sprintf(str,"stop %s",current_song_fname);
				mciSendString(str,"file");
				#endif //SOUND

				drawing=false;
				playing=false;
				dialogue=true;
				dialogue_rendered=false;
				gameover=true;
				printf("gameover\n");
			}

			//manage ens
			{
				int i;
				for(i=0;i<MAXENS;i++)
				{
					if(!en[i].exists) continue; //skip dead ens

					//hurts
					// if(pl.hurt_time<=0 && (int)(((pl.x)/16))==(int)(((en[i].x)/16)) && (int)(((pl.y)/16))==(int)(((en[i].y)/16)))
					if(en[i].type!=5 && pl.hurt_time<=0 && fabs(pl.x-en[i].x+8)<8 && fabs(pl.y-en[i].y)<8)
					{
						pl.hurt_time=40;
						pl.hp--;
						pl.hsp+=(pl.x-en[i].x)*fabs(1.0/(pl.x-en[i].x))*2.0; //knockback
						pl.vsp+=(pl.y-en[i].y)*fabs(1.0/(pl.y-en[i].y))*2.0;
						if(rand()%2==0) PlaySound("data/sfx/meow1.wav",NULL,SND_ASYNC); //two variations (!)
						else			PlaySound("data/sfx/meow2.wav",NULL,SND_ASYNC);
					}


					//move enemies
					MoveEnBasic(&en[i],&map,&pl);
					(*MoveEn[en[i].type])(&en[i],&map,&pl); //run enemy type's specific movement code
				}
			}

			//manage shots
			{
				int i;
				for(i=0;i<MAXSHOTS;i++)
				{
					if(!shots[i].exists) continue; //skip 'non-existant' shots
					shots[i].x+=shots[i].hsp;
					shots[i].y+=shots[i].vsp;
					if( (map.tile[aocoord((int)(((shots[i].x+8)/16)),(int)(((shots[i].y+8)/16)),MAPW)] >=2) || (shots[i].x<0 || shots[i].x>320 || shots[i].y<0 || shots[i].y>240))
						shots[i].exists=false;

					//player struck
					if(pl.exists && pl.hurt_time<=0 && shots[i].owner!=&pl && fabs(shots[i].x-pl.x)<8 && fabs(shots[i].y-pl.y-8)<8)
					{
						pl.hurt_time=40;
						shots[i].exists=false;

						float calcdmg=0.0;
						calcdmg=(float)(shots[i].type+1)*PLDEF; //PLDEF is a multiplier used to half (or theoretically quarter, etc.) the damage taken
						if((int)(calcdmg)==0) calcdmg=1.0; //don't allow zero damage

						pl.hp-=(int)(calcdmg);
						// pl.hp-=shots[i].type+1; //add one for type==0
						//printf("dmg:%i (pl hp:%i)\n",shots[i].type+1,pl.hp);
						if(pl.hp<=0 && !record_playback)
						pl.exists=false;
						if(rand()%2==0) PlaySound("data/sfx/meow1.wav",NULL,SND_ASYNC);
						else			PlaySound("data/sfx/meow2.wav",NULL,SND_ASYNC);
					}

					if(shots[i].owner==&pl && pl.exists) //only hit enemies if shot is from player
					{
						int iEn;
						for(iEn=0;iEn<MAXENS;iEn++)
						{
							if(!en[iEn].exists) continue; //skip dead enemies
							//struck
							// if((int)(((shots[i].x+8)/16))==(int)(((en[iEn].x)/16)) && (int)(((shots[i].y)/16))==(int)(((en[iEn].y+8)/16)))
							if(fabs(shots[i].x-en[iEn].x)<8 && fabs(shots[i].y-en[iEn].y-8)<8)
							{
								shots[i].exists=false;
								en[iEn].hp-=shots[i].type+1; //add one for type==0
								//printf("dmg:%i (en hp:%i)\n",shots[i].type+1,en[iEn].hp);
								if(en[iEn].hp<=0)
								{
									en[iEn].exists=false;
									if(en[iEn].type==5) //boox
									{
										playtheding=true;
										#ifdef SOUND
										char str[512];
										sprintf(str,"pause %s",current_song_fname);
										mciSendString(str,"file");
										#endif //SOUND
										sprintf(dialogue_text,"Game is saved!\nHP restored!\nPress C to continue");
										//playing=false;
										drawing=false;
										dialogue=true;
										pl.hp=MAXHP;
										dialogue_rendered=false;
										pl.hp=MAXHP;
										SaveGame(&pl,"data/profile.dat");
									}
									if(en[iEn].type==3) //create a save point!!!
									{
										//clear shots and enemies
										ClearShots();
										int i;
										for(i=0;i<MAXENS;i++)
											en[i].exists=false;
										CreateEnt(&en[0],en[iEn].x,en[iEn].y,5);
										en[0].obeysgrav=true;
										en[0].hp=5;

										#ifdef SOUND
										char str[512];
										sprintf(str,"pause %s",current_song_fname); //stop the music. it will reload on OpenMap
										mciSendString(str,"file");
										#endif //SOUND

										if(current_map==204) //message after original fight
										{
											sprintf(dialogue_text,"You've obtained the double jump!!!\nJump in-air to perform the double jump.\nPress C to continue");
											playing=false;
											drawing=false;
											dialogue=true;
											dialogue_rendered=false;
											djump=true; //yay
											playtheding=true;
										}


										firstbossdead++; //can be up to 2, because of the boss rush
										// firstbossdead=true;//first boss
										pl.hp=MAXHP;


										if(firstbossdead==2 && current_map==115)
										{
											map.tile[aocoord(19,8,MAPW)]=1;
											map.tile[aocoord(19,9,MAPW)]=1; //open the door after boss is dead!
											current_region=5;
										}
									}
									if(en[iEn].type==4) //create a save point!!!
									{
										//clear shots and enemies
										ClearShots();
										int i;
										for(i=0;i<MAXENS;i++)
											en[i].exists=false;
										CreateEnt(&en[0],en[iEn].x,en[iEn].y,5);
										en[0].obeysgrav=true;
										en[0].hp=5;


										#ifdef SOUND
										char str[512];
										sprintf(str,"pause %s",current_song_fname); //stop the music. it will reload on OpenMap
										mciSendString(str,"file");
										#endif //SOUND

										if(current_map==403) //message after original fight
										{
										playtheding=true;
										sprintf(dialogue_text,"You've obtained a weapon upgrade!\nPress C to continue");
										playing=false;
										drawing=false;
										dialogue=true;
										dialogue_rendered=false;
										shot_type=1; //weapon upgrade
										}

										if(current_map==403) secondbossdead=(secondbossdead==0)?(1):(2); //if you fight the original second boss after boss rush version, make sure it's still good
										if(current_map==117) secondbossdead=2; //the original second boss is technically optional, so eliminate chance of recurring boss rush appearance

										// secondbossdead++; //can be up to 2, because of the boss rush
										// secondbossdead=true;//second boss
										pl.hp=MAXHP;

										if(current_map==117 && secondbossdead==2)
										{
											map.tile[aocoord(19,4,MAPW)]=1; //open the door after boss is dead
											map.tile[aocoord(19,5,MAPW)]=1;
										}
									}
									if(en[iEn].type==8 && realthirdboss<2) //create a save point!!!
									{
										//clear shots and enemies
										ClearShots();
										int i;
										for(i=0;i<MAXENS;i++)
											en[i].exists=false;
										CreateEnt(&en[0],en[iEn].x,en[iEn].y,5);
										en[0].obeysgrav=true;
										en[0].hp=5;

										#ifdef SOUND
										char str[512];
										sprintf(str,"pause %s",current_song_fname); //stop the music. it will reload on OpenMap
										mciSendString(str,"file");
										#endif //SOUND

										if(current_map==8) //message after original fight
										{
										playtheding=true;
										sprintf(dialogue_text,"Suit upgrade!\nMobility enhanced! Damage reduced by 1/2!\nPress C to continue");
										playing=false;
										drawing=false;
										dialogue=true;
										dialogue_rendered=false;
										// shot_type=2; //best attack
										}


										if(current_map==217) //message after last boss rush fight
										{
										playtheding=true;
										sprintf(dialogue_text,"You beat the whooole game!\nThanks for playing!!!!!\nPress C to continue");
										playing=false;
										drawing=false;
										dialogue=true;
										dialogue_rendered=false;
										// shot_type=2; //best attack
										}


										thirdbossdead++; //can be up to 2, because of the boss rush
										// thirdbossdead=true;//third boss
										pl.hp=MAXHP;
										if(current_map==8)
										{
											map.tile[aocoord(19,6,MAPW)]=1;
											map.tile[aocoord(18,6,MAPW)]=0; //open the door after boss is dead!
											current_region=5;
										}
										if(current_map==217 && thirdbossdead==2)
										{
											map.tile[aocoord(0,5,MAPW)]=1;
											map.tile[aocoord(0,6,MAPW)]=1; //open the door after boss is dead!
										}
									}
									else if(en[iEn].type==8 && realthirdboss>0) realthirdboss--; //subtract a 'twin'
								}
								if(en[iEn].type==3 || en[iEn].type==4 || en[iEn].type==8) //boss hurt sounds
								{
									if(rand()%2==0) PlaySound("data/sfx/themow1.wav",NULL,SND_ASYNC); //two variations (!)
									else			PlaySound("data/sfx/themow2.wav",NULL,SND_ASYNC);
								}
								else
									PlaySound("data/sfx/hurt.wav",NULL,SND_ASYNC);
							}
						}
					}
				}
			}
		}

		if(dialogue)
		{
			if(playtheding)
			{
				PlaySound("data/sfx/yaysound.wav",NULL,SND_ASYNC);
				playtheding=false;
			}
			if(tigrKeyDown(screen,TK_c)) //user progresses text/ends dialogue
			{
					dialogue=false;
					playing=true;
					drawing=true;
					title=false;
				if(!gameover)
				{
					#ifdef SOUND
					char str[512];
					sprintf(str,"play %s repeat",current_song_fname);
					mciSendString(str,"file");
						if( (current_map==204 && firstbossdead!=0) || (current_map==115 && firstbossdead==2) ) mciSendString("stop","data/sfx/beatem.mp3");
						if( (current_map==403 && secondbossdead!=0) || (current_map==117 && secondbossdead==2) ) mciSendString("stop","data/sfx/beatem.mp3");
						if( (current_map==8 && thirdbossdead!=0) || (current_map==217 && thirdbossdead==2) ) mciSendString("stop","data/sfx/beatem.mp3");
					#endif //SOUND
				}
				if(gameover)
				{
					Reset(&pl,&map);
					LoadGame(&pl,&map,"data/profile.dat");

					// Reset(&pl,&map); //do this first, in case load fails
					// printf("%i\n",LoadGame(&pl,&map,"data/profile.dat"));
					// if(LoadGame(&pl,&map,"data/profile.dat")) Reset(&pl,&map); //reset if load failed
					// printf("game restarted\n");
					ClearShots();
					gameover=false; //I forgot this and it was a problem

					#ifdef SOUND

					if(!title)
					{
						SetMusic();
					}

					if(current_map==204 && firstbossdead) mciSendString("stop","data/sfx/beatem.mp3");
					if(current_map==403 && secondbossdead) mciSendString("stop","data/sfx/beatem.mp3");
					if(current_map==8   && thirdbossdead) mciSendString("stop","data/sfx/beatem.mp3");

					#endif //SOUND

					// #ifdef SOUND
					// mciSendString("play","data/sfx/upsanddowns.mp3");
					// current_song_fname="data/sfx/upsanddowns.mp3";
					// #endif //SOUND
				}
				//printf("'C':%x\n",'C');
			}

			//render (dialogue)-----
			if(!dialogue_rendered)
			{
				tigrBlitAlpha(screen,spr_tb,0,140,0,0,320,100,1.0);
				//tigrPrint(screen,tfont,40,150,tigrRGB(0xaa,0xaa,0xaa),dialogue_text);
				dialogue_rendered=true;
			}
		}


		//progress recording frames
		if(recording || record_playback)
		{
			if(current_frame<RECORDLEN)
			{
				// printf("%x\n",recorded_input[current_frame]);
				current_frame++; //advance frames until RECORDLEN end
			}
			else
			{
				//when we advance to end of recording, set recording and/or record_playback to false
				// recording=false;
				// record_playback=false;
				// current_frame=0; //'rewind' recording

				//restart

				current_recording=(current_recording+1)%TOTALRECORDINGS;
				char str[128];
				sprintf(str,"data/rec/p%i.dat",current_recording);
				LoadGame(&pl,&map,str);
				sprintf(str,"data/rec/rec%i.dat",current_recording);
				LoadRec(str);
				ClearShots();

				current_frame=0;
				recording=false;
				record_playback=true;
				srand(RECORDRNGVAL);
			}
		}

		if(drawing)
		// if(true)
		{
			//render-----
			tigrClear(screen, bg_color);

			//draw map
			{
				int i;
				for(i=0;i<MAPMAXTILES;i++)
					if(map.tile[i]) tigrBlitAlpha(screen,spr_tile[map.tile[i]],aotox(i,MAPW)*16,aotoy(i,MAPW)*16+cam_y,0,0,16,16,1.0);
			}

			//draw shots
			{
				int i;
				for(i=0;i<MAXSHOTS;i++)
				{
					if(!shots[i].exists) continue; //skip 'non-existant' shots
					tigrBlitAlpha(screen,spr_shot[shots[i].type],(int)(shots[i].x)+4,(int)(shots[i].y)+2+cam_y,0,0,16,16,1.0);
					// tigrBlitAlpha(screen,spr_tile[0],(int)((shots[i].x+24)/16)*16,(int)((shots[i].y+8)/16)*16,0,0,16,16,1.0);
				}
			}

			//draw enemies
			{
				int i;
				for(i=0;i<MAXENS;i++)
				{
					if(!en[i].exists) continue; //skip dead ens
					tigrBlitAlpha(screen,spr_en[en[i].type],(int)(en[i].x+6),(int)(en[i].y+4)+cam_y,0,0,16,16,1.0);
					// tigrBlitAlpha(screen,spr_tile[0],(int)((en[i].x-8)/16)*16,(int)((en[i].y+8)/16)*16,0,0,16,16,1.0);
				}
			}

			//draw player
			if( (pl.exists && pl.hurt_time<=0) || (pl.exists && pl.hurt_time && rand()%2==0) ) //check if exists (and flicker if hurt)
				tigrBlitAlpha(screen,spr_pl[pl_dir*2+(pl_anim/(MAXANIM/2))*on_ground],(int)(pl.x)+8,(int)(pl.y)+8+cam_y,0,0,16,16,1.0); //draw player
			// tigrPrint(screen, tfont, 120, 110, tigrRGB(0xff, 0xff, 0xff), "words");

			//draw recording status
			#ifdef RECORDER
			{
				char str[128];
				sprintf(str,"rec(%c):%i/%i",recording?'y':'n',current_frame,RECORDLEN);
				tigrPrint(screen,tfont,150,0,tigrRGB(0xff,recording?0x00:0xff,recording?0x00:0xff),str);
			}
			#endif //RECORDER

			//draw hp
			// if(pl.hp>0)
			if(!title) //don't draw on title intro
			{
				/*char str[512];
				memset(str,'\0',512); //fill with null chars
				sprintf(str,"hp: ");
				int i;
				for(i=0;i<MAXHP;i++)
					if(pl.hp>i)
						str[i+4]='|';*/
				//tigrPrint(screen,tfont,50,0,tigrRGB(0xff,0x00,0x61),"hp:");
				int i;
				for(i=0;i<MAXHP;i++)
					tigrBlitAlpha(screen,spr_hp[!(pl.hp>i)],70+i*8+i+1,0,0,0,8,8,1.0); //draw status of hp at each unit
			}

			if(title) //drawing title intro screen
			{
				tigrFill(screen,0,0,320,40,(TPixel){0});
				tigrFill(screen,0,90,320,150,(TPixel){0});

				title_xoff=(title_xoff-1)%320;

				//DrawSineTitle(screen,spr_title,140+title_xoff,62);
				//DrawSineTitle(screen,spr_title,140+title_xoff+320,60);

				//tigrPrint(screen, tfont, 140, 130, tigrRGB(0xff, 0xff, 0xff), "Z to Start New\nC to Continue Last");
				//tigrPrint(screen, tfont, 250, 220, tigrRGB(0xff, 0xff, 0xcc), "pvtroswold");
			}

			//draw editor things
			#ifdef EDITOR
				// tigrBlitAlpha(screen,spr_tile[0],(int)((pl.x+16)/16)*16,(int)((pl.y+16)/16)*16,0,0,16,16,1.0); //where is player
				char str[512];
				sprintf(str,"t:%i\nmap:%i\nreg:%i",edittype,current_map,current_region);
				tigrPrint(screen, tfont, 0,0, tigrRGB(0xff, 0xff, 0xff), str);
				if(cursx>0 && cursy>0) //don't divide by zero!
					tigrBlitAlpha(screen,spr_tile[0],(cursx/16)*16,(cursy/16)*16,0,0,16,16,1.0); //draw editor cursor
			#endif //EDITOR

			// tigrUpdate(screen); //done in tigr window while loop
		}
		// Sleep(10); //keep from lagging??? possibly give game less practical priority??? ??
		tigrUpdate(screen);
		if(quit_game)break;
		usleep(16000);
	}

	for(int i=0;i<6;++i)
		if(spr_tile[i])
		{
			if(spr_tile[i]->tex)
				free(spr_tile[i]->tex);
			free(spr_tile[i]);
		}
	for(int i=0;i<4;++i)
		if(spr_shot[i])
		{
			if(spr_shot[i]->tex)
				free(spr_shot[i]->tex);
			free(spr_shot[i]);
		}
	for(int i=0;i<20;++i)
		if(spr_en[i])
		{
			if(spr_en[i]->tex)
				free(spr_en[i]->tex);
			free(spr_en[i]);
		}
	for(int i=0;i<4;++i)
		if(spr_pl[i])
		{
			if(spr_pl[i]->tex)
				free(spr_pl[i]->tex);
			free(spr_pl[i]);
		}
	for(int i=0;i<2;++i)
		if(spr_hp[i])
		{
			if(spr_hp[i]->tex)
				free(spr_hp[i]->tex);
			free(spr_hp[i]);
		}

	puts("bye");
	tigrFree(screen);
}
