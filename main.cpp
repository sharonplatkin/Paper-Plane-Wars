#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <vector>
#include <time.h>
#include <algorithm>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#ifdef __APPLE__
#  include <OpenGL/gl.h>
#  include <OpenGL/glu.h>
#  include <GLUT/glut.h>
#else
#  include <GL/gl.h>
#  include <GL/glu.h>
#  include <GL/freeglut.h>
#endif

#include "Plane.h"
#include "Human.h"

#define X 0
#define Y 1
#define Z 2
#define W 3

float camPos[] = {0, 8, 10};	//where the camera is
float camTarget[] = {0, 7, 0};

GLubyte* image; // paper texture
int width, height;
GLubyte* image2; // grass texture
int width2, height2;

GLubyte* image3; // you lose texture
int width3, height3;

GLubyte* heart; // heart textures
int widthHeart, heightHeart;
GLubyte* empty;
int widthEmpty, heightEmpty;

std::vector<Plane*> PlaneList;
int selectedPlane = -1; // selected object ID, -1 if nothing selected
int highlight = -1;
int highlightTimer = 0;

int gameState = 0; // 0=none, 1=pitch, 2=yaw, 3=power, 4=inFlight
int wheelTimer;

bool select = false;
double* m_start = new double[3]; // ray-casting coords
double* m_end = new double[3];
int _X, _Y;

Plane *CompPlane; // computer's plane

Human *Player; // player model
Human *Computer; // computer model

int stateTimer;

GLubyte* LoadPPM(const char* file, int* width, int* height) {
    GLubyte* img;
    FILE *fd;
    int n, m;
    int  k, nm;
    char c;
    int i;
    char b[100];
    float s;
    int red, green, blue;
    
    /* first open file and check if it's an ASCII PPM (indicated by P3 at the start) */
    fd = fopen(file, "r");
    fscanf(fd,"%[^\n] ",b);
    if(b[0]!='P'|| b[1] != '3')
    {
        printf("%s is not a PPM file!\n",file);
        exit(0);
    }
    fscanf(fd, "%c",&c);
    
    /* next, skip past the comments - any line starting with #*/
    while(c == '#')
    {
        fscanf(fd, "%[^\n] ", b);
        printf("%s\n",b);
        fscanf(fd, "%c",&c);
    }
    ungetc(c,fd);
    
    /* now get the dimensions and max colour value from the image */
    fscanf(fd, "%d %d %d", &n, &m, &k);
    
    /* calculate number of pixels and allocate storage for this */
    nm = n*m;
    img = (GLubyte*)malloc(3*sizeof(GLuint)*nm);
    s=255.0/k;
    
    /* for every pixel, grab the read green and blue values, storing them in the image data array */
    for(i=0;i<nm;i++)
    {
        fscanf(fd,"%d %d %d",&red, &green, &blue );
        img[3*nm-3*i-3]=red*s;
        img[3*nm-3*i-2]=green*s;
        img[3*nm-3*i-1]=blue*s;
    }
    
    /* finally, set the "return parameters" (width, height, max) and return the image array */
    *width = n;
    *height = m;
    
    return img;
}

void mouse(int btn, int state, int x, int y){
	int centerX = glutGet(GLUT_WINDOW_WIDTH)/2;
	int centerY = glutGet(GLUT_WINDOW_HEIGHT)/2;

	if (btn == GLUT_LEFT_BUTTON && state == GLUT_DOWN && gameState == 0 && Player->getHealth()!=0){
		select = true;
		_X = x;
		_Y = y;
	}
}

std::vector<float> RayTest(int objID, std::vector<Plane*>::iterator obj) {
	double matModelView[16], matProjection[16];
	int viewport[4];

	glGetDoublev(GL_MODELVIEW_MATRIX, matModelView);
	glGetDoublev(GL_PROJECTION_MATRIX, matProjection);
	glGetIntegerv(GL_VIEWPORT, viewport);

	double winX = (double)_X;
	double winY = viewport[3] - (double)_Y;

	gluUnProject(winX, winY, 0.0, matModelView, matProjection, viewport, &m_start[X], &m_start[Y], &m_start[Z]);
	gluUnProject(winX, winY, 1.0, matModelView, matProjection, viewport, &m_end[X], &m_end[Y], &m_end[Z]);

	double* R0 = new double[3];
	double* Rd = new double[3];

	double xDiff = m_end[X] - m_start[X];
	double yDiff = m_end[Y] - m_start[Y];
	double zDiff = m_end[Z] - m_start[Z];

	double mag = sqrt(xDiff*xDiff + yDiff*yDiff + zDiff*zDiff);
	R0[X] = m_start[X];
	R0[Y] = m_start[Y];
	R0[Z] = m_start[Z];

	Rd[X] = xDiff / mag;
	Rd[Y] = yDiff / mag;
	Rd[Z] = zDiff / mag;

	std::vector<float> intersectRet;
	std::vector< std::vector<float> > faceNorms = (*obj)->getBoundFaceNorms();
	std::vector<float> faceDists = (*obj)->getBoundFaceDists();

	for (int i=0; i<6; i++) {
		double NRd = faceNorms[i][X]*Rd[X] + faceNorms[i][Y]*Rd[Y] + faceNorms[i][Z]*Rd[Z];

		// only count intersection with front of face (only testing 3 faces)
		if (NRd < 0) {
			double NR0 = faceNorms[i][X]*R0[X] + faceNorms[i][Y]*R0[Y] + faceNorms[i][Z]*R0[Z];
			double t = -(NR0 + faceDists[i]) / NRd;

			double point[] = {R0[X] + t*Rd[X], R0[Y] + t*Rd[Y], R0[Z] + t*Rd[Z]};

			// ray distance info (only used when in-bounds test passes)
			double xtoP = R0[X] - point[X];
			double ytoP = R0[Y] - point[Y];
			double ztoP = R0[Z] - point[Z];

			double magtoP = sqrt(xtoP*xtoP + ytoP*ytoP + ztoP*ztoP); // distance from camera to intersection point

			// check if intersection point is within bounds!!!
			if (((i==0 || i==3) && point[Y]>=faceDists[1] && point[Y]<=-faceDists[4] && point[Z]>=faceDists[2] && point[Z]<=-faceDists[5]) ||
				((i==1 || i==4) && point[X]>=faceDists[0] && point[X]<=-faceDists[3] && point[Z]>=faceDists[2] && point[Z]<=-faceDists[5]) ||
				((i==2 || i==5) && point[X]>=faceDists[0] && point[X]<=-faceDists[3] && point[Y]>=faceDists[1] && point[Y]<=-faceDists[4])) {
				// intersection in-bounds! Add distance and object ID to list
				intersectRet.push_back(magtoP);
				intersectRet.push_back((float)objID);
			}
		}
	}

	return intersectRet;
}

// determining closest intersection from list of intersections
void DetermineSelection(std::vector<float> IntersectList) {
	if (IntersectList.size()>0) {
		float minVal = 900.0;
		float minID;

		std::vector<float>::iterator it2 = IntersectList.begin(); 
		while(it2 != IntersectList.end()) {
			//replace minVal with closest intersection
			if (*it2 < minVal) {
				minVal = *it2;
				it2++;
				minID = *it2;
			} else {
				it2++;
			}
			it2++;
		}

		if (minID>=3) {
			return;
		}

		if (highlight == PlaneList[minID]->getID()) { // if the cursor is over the same plane as it was last cycle
			highlightTimer++;
		}
		highlight = PlaneList[minID]->getID();

		if (select && (int)minID!=selectedPlane) {
			if (selectedPlane!=-1) {
				PlaneList.pop_back();
			}

			selectedPlane = (int)minID;

			Plane *newPlane = new Plane();
			char filename[] = "plane_1.obj";
			filename[6] = selectedPlane+'1';
			newPlane->InitPlane(PlaneList.size(), filename, -0.001, 0, 5, 0, 0, 0);
			PlaneList.push_back(newPlane);
		}
	} else { 
		// if no intersections after ray test, reset stuff
		highlight = -1;
		highlightTimer = 0;
	}

	select = false;
}

void mouseMotion(int x, int y){

}

void mousePassiveMotion(int x, int y){
	if (gameState==0) {
		_X = x;
		_Y = y;
	}
}

//keyboard stuff
void keyboard(unsigned char key, int xIn, int yIn) {
	int mod = glutGetModifiers();
	switch (key) {
		case 'q':
		case 27:	//27 is the esc key
			exit(0);
			break;

		case 's':
			camTarget[Y] -= 0.1f;
			break;

		case 'w':
			camTarget[Y] += 0.1f;
			break;

		case 'a':
			camTarget[X] -= 0.1f;
			break;

		case 'd':
			camTarget[X] += 0.1f;
			break;

		case 'r':
			while (PlaneList.size()>3) {
				PlaneList.pop_back();
			}
			Player = new Human();
			Player->InitHuman(0, 0, 3, 10);
			Computer = new Human();
			Computer->InitHuman(1, 0, 3, -60);
			selectedPlane = -1;
			wheelTimer = -30;
			gameState = 0;
			break;

		case ' ':
			if (gameState<4 && selectedPlane!=-1) {
				switch (gameState) {
					case 0: // starts aiming process
						wheelTimer = 0;
						gameState = 1;
						break;

					case 1: // pitch set
						wheelTimer = 25;
						gameState = 2;
						break;

					case 2: // yaw set
						wheelTimer = -30;
						gameState = 3;
						break;

					case 3: // power set, launches
						wheelTimer = -30;
						gameState = 4;
						PlaneList[PlaneList.size()-1]->inFlight = true;
						PlaneList[PlaneList.size()-1]->LaunchPlane();
						break;
				}
			}
			break;
	}
}

void special(int key, int xIn, int yIn){
	if (gameState==4) {
		switch (key){
			case GLUT_KEY_DOWN:
				PlaneList[PlaneList.size()-1]->BlowPlane(0,-0.001);
				break;

			case GLUT_KEY_UP:
				PlaneList[PlaneList.size()-1]->BlowPlane(0,0.001);			
				break;

			case GLUT_KEY_LEFT:
				PlaneList[PlaneList.size()-1]->BlowPlane(-0.001, 0);
				break;

			case GLUT_KEY_RIGHT:
				PlaneList[PlaneList.size()-1]->BlowPlane(0.001, 0);
				break;
		}
	} else if (gameState==5) {
		switch (key){
			case GLUT_KEY_LEFT:
				Player->MoveHuman(-0.05);
				break;

			case GLUT_KEY_RIGHT:
				Player->MoveHuman(0.05);
				break;
		}
	}
}

//menu stuff
void menuProc(int value){
	if (value == 1)
		printf("First item was clicked\n");
}

void createOurMenu(){
	//int subMenu_id = glutCreateMenu(menuProc2);
	int subMenu_id = glutCreateMenu(menuProc);
	glutAddMenuEntry("gahh", 3);
	glutAddMenuEntry("gahhhhhh", 4);
	glutAddMenuEntry("????", 5);
	glutAddMenuEntry("!!!!!", 6);

	int main_id = glutCreateMenu(menuProc);
	glutAddMenuEntry("First item", 1);
	glutAddMenuEntry("Second item", 2);
	glutAddSubMenu("pink flamingo", subMenu_id);
	glutAttachMenu(GLUT_RIGHT_BUTTON);
}

void FloorMesh() {
	// glDisable(GL_LIGHTING);
	// glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glEnable(GL_LIGHTING);
	glPolygonMode(GL_FRONT, GL_FILL);
	glCullFace(GL_BACK);
	// glLineWidth(1);

	float leaf_ambi[4] = {0.0,0.3,0.0,1}; //ambient light
	float leaf_diff[3] = {0.1,0.7,0.1}; //shadows casting
	float leaf_spec[3] = {0.04,0.04,0.04};
	float leaf_shin = 0.078125;
	
    glMaterialfv(GL_FRONT, GL_AMBIENT, leaf_ambi);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, leaf_diff);
	glMaterialfv(GL_FRONT, GL_SPECULAR, leaf_spec);
	glMaterialf(GL_FRONT, GL_SHININESS, leaf_shin * 128.0);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width2, height2, 0, GL_RGB, GL_UNSIGNED_BYTE, image2);

	for (int i=20; i>-100-1; i--) {
		glBegin(GL_QUAD_STRIP);
			for (int j=50; j>-50; j--) {
				glNormal3f(0,1,0);
				glTexCoord2f((j+50.0)/100.0, i/-120.0);
				glVertex3f(j, 0, i);
				glNormal3f(0,1,0);
				glTexCoord2f((j+50.0)/100.0, i/-120.0);
				glVertex3f(j, 0, i-1);
			}
		glEnd();
	}
}

void DrawTree(int posX, int posZ){
	glEnable(GL_LIGHTING);
	glPolygonMode(GL_FRONT, GL_FILL);

	glPushMatrix();
		glTranslatef(posX,6,posZ);
		glPushMatrix();
			glRotatef(90,1,0,0);
			glColor3f(0.5,0.2,0.2);	

			float wood_ambi[4] = {0.2,0.08,0.0,1}; //ambient light
			float wood_diff[3] = {0.5, 0.3,0.1}; //shadows casting
			float wood_spec[3] = {0.7,0.4,0.04};
			float wood_shin = 0.078125;
		    glMaterialfv(GL_FRONT, GL_AMBIENT, wood_ambi);
		    glMaterialfv(GL_FRONT, GL_DIFFUSE, wood_diff);
			glMaterialfv(GL_FRONT, GL_SPECULAR, wood_spec);
			glMaterialf(GL_FRONT, GL_SHININESS, wood_shin * 128.0);

			gluCylinder(gluNewQuadric(),1,1,6,100,10);
		glPopMatrix();

		glTranslatef(0,5,0);
		glRotatef(90,1,0,0);
		glColor3f(0,0.7,0.2);

		
		float leaf_ambi[4] = {0.0,0.3,0.0,1}; //ambient light
		float leaf_diff[3] = {0.1,0.9,0.1}; //shadows casting
		float leaf_spec[3] = {0.04,0.04,0.04};
		float leaf_shin = 0.078125;
		
	    glMaterialfv(GL_FRONT, GL_AMBIENT, leaf_ambi);
	    glMaterialfv(GL_FRONT, GL_DIFFUSE, leaf_diff);
		glMaterialfv(GL_FRONT, GL_SPECULAR, leaf_spec);
		glMaterialf(GL_FRONT, GL_SHININESS, leaf_shin * 128.0);

		gluCylinder(gluNewQuadric(),0.1, 1, 2, 20, 20);
		gluCylinder(gluNewQuadric(),0.1, 1.5, 3.75, 20, 20);
		gluCylinder(gluNewQuadric(),0.1, 2, 5.75, 20, 20);
		gluCylinder(gluNewQuadric(),0.1, 2.5, 8, 20, 20);
	glPopMatrix();
}

void LaunchSequence(std::vector<Plane*>::iterator obj) {
	glDisable(GL_LIGHTING);
	wheelTimer++;
	if (wheelTimer>50) {
		wheelTimer = -50;
	}

	switch (gameState) {
		case 1:
			(*obj)->SetPitch(wheelTimer);
			break;

		case 2:
			(*obj)->SetYaw(wheelTimer);
			break;

		case 3:
			(*obj)->SetPower(wheelTimer);
			break;
	}

	// Show launch vector
	glLineWidth(5);
	glBegin(GL_QUADS);
		glColor3f(0,0,0);
		glVertex3f(0,0,0);
		if (gameState==3) {
			glVertex3f(0.5,0,(*obj)->getPower()*-16 + 3.5);
			glVertex3f(0,0,(*obj)->getPower()*-20 + 3.5);
			glVertex3f(-0.5,0,(*obj)->getPower()*-16 + 3.5);
		} else {
			glVertex3f(0.5,0,-12.8);
			glVertex3f(0,0,-16);
			glVertex3f(-0.5,0,-12.8);
		}
	glEnd();
	glLineWidth(1);
}

void Prepare3D() {
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(45, (float)((glutGet(GLUT_WINDOW_WIDTH)+0.0f)/glutGet(GLUT_WINDOW_HEIGHT)), 1, 300);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity(); 
	glEnable(GL_TEXTURE_2D);
}

void Prepare2D() {
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluOrtho2D(0, glutGet(GLUT_WINDOW_WIDTH), 0, glutGet(GLUT_WINDOW_HEIGHT));
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glDisable(GL_LIGHTING);
	glEnable(GL_TEXTURE_2D);
}

void DrawEnvironment() {
	FloorMesh();
	DrawTree(10,-80); //params xpos, zpos
	DrawTree(-30,-15);
	DrawTree(-47,-10);
	DrawTree(-47,-94);
	DrawTree(47,-94);
	DrawTree(40,-50);
}

void PrepareLaunch() {
	gluLookAt(camPos[0], camPos[1], camPos[2], camTarget[0], camTarget[1], camTarget[2], 0,1,0);

	DrawEnvironment();

	std::vector<float> IntersectList;

	for (std::vector<Plane*>::iterator it = PlaneList.begin(); it != PlaneList.end(); it++) {
		if (gameState==0 || (*it)->getID()==PlaneList.size()-1) {
			glPushMatrix();
				std::vector<float> translateCoords = (*it)->getCoords();
				glTranslatef(translateCoords[0], translateCoords[1], translateCoords[2]);

				std::vector<float> rotateAngles = (*it)->getOrient();
				glRotatef(rotateAngles[0], 1, 0, 0); // x-axis rotation (pitch)
				glRotatef(rotateAngles[1], 0, 1, 0); // y-axis rotation (yaw)

				glPushMatrix();
					glPushMatrix();
						glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
						glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
						glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
						glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
						glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
						if ((*it)->getID() == PlaneList.size()-1) {
							(*it)->DrawPlane(true);
						} else {
							glScalef(0.75, 0.75, 0.75);
							if (highlight == (*it)->getID() && highlight<3 && (*it)->getID() < 3) {
								(*it)->ExhibitPlane(highlightTimer);
							}
							(*it)->DrawPlane(highlight == (*it)->getID());
						}
					glPopMatrix();

					if (gameState==0) {
						std::vector<float> intersection = RayTest((*it)->getID(), it);
						if (intersection.size() != 0) {
							IntersectList.push_back(intersection[0]);
							IntersectList.push_back(intersection[1]);
						}
					}
				glPopMatrix();

				if (gameState != 0) {
					LaunchSequence(it);
					
				}

			glPopMatrix();
		}
	}

	if (gameState==0) {
		DetermineSelection(IntersectList);
	}
}

void FollowPlane() {
	std::vector<float> translateCoords = PlaneList[PlaneList.size()-1]->getCoords();
	gluLookAt(translateCoords[0], translateCoords[1]+3, translateCoords[2]+10, translateCoords[0], translateCoords[1]+2, translateCoords[2], 0,1,0);

	DrawEnvironment();

	glPushMatrix();
		glTranslatef(translateCoords[0], translateCoords[1], translateCoords[2]);
		std::vector<float> rotateAngles = PlaneList[PlaneList.size()-1]->getOrient();
		glRotatef(rotateAngles[0], 1, 0, 0); // x-axis rotation (pitch)
		glRotatef(rotateAngles[1], 0, 1, 0); // y-axis rotation (yaw)
		glRotatef(rotateAngles[2], 0, 0, 1); // z-axis rotation (yaw)

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
		PlaneList[PlaneList.size()-1]->DrawPlane(true);
	glPopMatrix();
	
	PlaneList[PlaneList.size()-1]->MovePlane();

}

void TestCollisionOut() {
	std::vector<float> planePos = PlaneList[PlaneList.size()-1]->getCoords();
	std::vector<float> planeBox = PlaneList[PlaneList.size()-1]->getBoundFaceDists();
	planeBox[3] *= -1;
	planeBox[4] *= -1;
	planeBox[5] *= -1;
	std::vector<float> compPos = Computer->getCoords();
	std::vector<float> compBox = Computer->getHitBox();

	if (planePos[0]+planeBox[0] < compPos[0]+compBox[3] && planePos[0]+planeBox[3] > compPos[0]+compBox[0]) {
		if (planePos[1]+planeBox[1] < compPos[1]+compBox[4] && planePos[1]+planeBox[4] > compPos[1]+compBox[1]) {
			if (planePos[2]+planeBox[2] < compPos[2]+compBox[5] && planePos[2]+planeBox[5] > compPos[2]+compBox[2]) {

				// HIT //
				PlaneList[PlaneList.size()-1]->Collision();
				stateTimer = 100;
			}
		}
	}

	if (planePos[1]<0) {
		PlaneList[PlaneList.size()-1]->Collision();
		stateTimer = 100;
	}
}

void TestCollisionIn() {
	std::vector<float> planePos = CompPlane->getCoords();
	std::vector<float> planeBox = CompPlane->getBoundFaceDists();
	planeBox[3] *= -1;
	planeBox[4] *= -1;
	planeBox[5] *= -1;
	std::vector<float> playPos = Player->getCoords();
	std::vector<float> playBox = Player->getHitBox();

	if (planePos[0]+planeBox[0] < playPos[0]+playBox[3] && planePos[0]+planeBox[3] > playPos[0]+playBox[0]) {
		if (planePos[1]+planeBox[1] < playPos[1]+playBox[4] && planePos[1]+planeBox[4] > playPos[1]+playBox[1]) {
			if (planePos[2]+planeBox[2] < playPos[2]+playBox[5] && planePos[2]+planeBox[5] > playPos[2]+playBox[2]) {

				// HIT //
				Player->TakeDamage();
				CompPlane->Collision();
				stateTimer = 100;
			}
		}
	}

	if (planePos[1]<0) {
		CompPlane->Collision();
		stateTimer = 100;
	}
}

void ControlPlayer() {
	gluLookAt(camPos[0], camPos[1]-1, camPos[2]+15, camTarget[0], camTarget[1]-1, camTarget[2]+15, 0,1,0);

	DrawEnvironment();

	glPushMatrix();
		std::vector<float> translateCoords = CompPlane->getCoords();
		std::vector<float> rotateAngles = CompPlane->getOrient();
		glTranslatef(translateCoords[0], translateCoords[1], translateCoords[2]);
		glRotatef(rotateAngles[0], 1, 0, 0); // x-axis rotation (pitch)
		glRotatef(rotateAngles[1], 0, 1, 0); // y-axis rotation (yaw)
		glRotatef(rotateAngles[2], 0, 0, 1); // z-axis rotation (yaw)
		
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
		CompPlane->DrawPlane(true);
		CompPlane->MovePlane();
	glPopMatrix();
}

void DisplayThrownPlanes() {
	for (int i=3; i<PlaneList.size(); i++) {
		if (i<PlaneList.size()-1 || gameState>4) {
			glPushMatrix();
				std::vector<float> translateCoords = PlaneList[i]->getCoords();
				glTranslatef(translateCoords[0], translateCoords[1], translateCoords[2]);

				std::vector<float> rotateAngles = PlaneList[i]->getOrient();
				glRotatef(rotateAngles[0], 1, 0, 0); // x-axis rotation (pitch)
				glRotatef(rotateAngles[1], 0, 1, 0); // y-axis rotation (yaw)

				glPushMatrix();
					glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
					glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
					glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
					glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
					glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					PlaneList[i]->DrawPlane(false);
				glPopMatrix();
			glPopMatrix();
		}
	}
}

void display(void) {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	Prepare3D();	

	if (gameState<4) {
		PrepareLaunch();
	} else if (gameState==4) { // gameState == 4
		FollowPlane();

		if (stateTimer==0) {
			TestCollisionOut();
			Computer->DodgePlane(PlaneList[PlaneList.size()-1]->getCoords());
		}
	} else if (gameState==5) { // comp returns plane, player dodges
		ControlPlayer();

		if (stateTimer==0) {
			TestCollisionIn();
		}
	}

	glPushMatrix();
		Player->DrawHuman();
	glPopMatrix();
	glPushMatrix();
		Computer->DrawHuman();
	glPopMatrix();
	glPushMatrix();
		DisplayThrownPlanes();
	glPopMatrix();
	
	if (stateTimer>0) {
		stateTimer--;
		if (stateTimer==0) {
			gameState++;

			if (gameState==5) {
				srand(time(NULL));
				CompPlane = new Plane();
				char filename[] = "plane_1.obj";
				filename[6] = PlaneList.size()%3+'1';
				CompPlane->InitPlane(-1, filename, -0.001, 0, 3.5, -59, rand()%7+5, rand()%4+178);
				CompPlane->SetPower(4);
				CompPlane->inFlight = true;
				CompPlane->LaunchPlane();
			} else if (gameState==6) {
				gameState = 0; // restart round
				selectedPlane = -1;
				Player->ResetPos();
				Computer->ResetPos();
			}
		}
	}

	glFlush();
	Prepare2D();

	glCullFace(GL_BACK);

	for (int i=0; i<3; i++) {
		if (Player->getHealth() > i) {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, widthHeart, heightHeart, 0, GL_RGB, GL_UNSIGNED_BYTE, heart);
		} else {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, widthEmpty, heightEmpty, 0, GL_RGB, GL_UNSIGNED_BYTE, empty);
		}

		glBegin(GL_QUADS);
		 	glColor3f(1,1,1);
		 	glTexCoord2f(0,0);
		 	glVertex2f(i*40, 0);
		 	glTexCoord2f(1,0);
		 	glVertex2f(i*40+40, 0);
		 	glTexCoord2f(1,1);
		 	glVertex2f(i*40+40, 40);
		 	glTexCoord2f(0,1);
			glVertex2f(i*40, 40);
		glEnd();
	}

	if (Player->getHealth() == 0) {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width3, height3, 0, GL_RGB, GL_UNSIGNED_BYTE, image3);

		glBegin(GL_QUADS);
			glColor3f(1,1,1);
			glTexCoord2f(0,0);
			glVertex2f(300, 150);
			glTexCoord2f(1,0);
			glVertex2f(600, 150);
			glTexCoord2f(1,1);
			glVertex2f(600, 450);
			glTexCoord2f(0,1);
			glVertex2f(300, 450);
		glEnd();
	}

	//flush out to single buffer
	glutSwapBuffers();
}

void reshape(int w, int h)
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(45, (float)((w+0.0f)/h), 1, 300);

	glMatrixMode(GL_MODELVIEW);
	glViewport(0, 0, w, h);
}

void FPSTimer(int value){ //60fps
	glutTimerFunc(17, FPSTimer, 0);
	glutPostRedisplay();
}

//initialization
void init(void)
{
	glClearColor(0.65, 0.65, 1.0, 1);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(45, (float)((glutGet(GLUT_WINDOW_WIDTH)+0.0f)/glutGet(GLUT_WINDOW_HEIGHT)), 1, 300);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glShadeModel(GL_SMOOTH);

	float lPos0[4] = {0, 100, 0, 0};
	float ambi0[4] = {0.9, 0.9, 0.9, 1};
	float diff0[4] = {0.9, 0.9, 0.9, 1};
	float spec0[4] = {0.1, 0.1, 0.1, 0};

	glLightfv(GL_LIGHT0, GL_POSITION, lPos0);
	glLightfv(GL_LIGHT0, GL_AMBIENT, ambi0);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, diff0);
	glLightfv(GL_LIGHT0, GL_SPECULAR, spec0);

	float density = 0.005;
	float fogColor[4] = {0.5, 0.5, 0.5, 1};

	glEnable(GL_FOG);
	glFogi(GL_FOG_MODE, GL_EXP2);
	glFogfv(GL_FOG_COLOR, fogColor);
	glFogf(GL_FOG_DENSITY, density);
	glHint(GL_FOG_HINT, GL_NICEST);
	glEnable(GL_TEXTURE_2D);

	image = LoadPPM("papertex.ppm", &width, &height);
	image2 = LoadPPM("grass.ppm", &width2, &height2);
	image3 = LoadPPM("youLose.ppm", &width3, &height3);
	heart = LoadPPM("fullheart2.ppm", &widthHeart, &heightHeart);
	empty = LoadPPM("emptyheart2.ppm", &widthEmpty, &heightEmpty);

	for (int i=0; i<3; i++) {
		Plane *newPlane = new Plane();
		char filename[] = "plane_1.obj";
		filename[6] = i+'1';
		newPlane->InitPlane(PlaneList.size(), filename, -0.001, i*3.5-3.5, 3.5, 0.5, 0, 0);
		PlaneList.push_back(newPlane);
	}

	remove("launchdata.txt");

	Player = new Human();
	Player->InitHuman(0, 0, 3, 10);
	Computer = new Human();
	Computer->InitHuman(1, 0, 3, -60);

	// ray-casting infos
	m_start[X] = 0;
	m_start[Y] = 0;
	m_start[Z] = 0;

	m_end[X] = 0;
	m_end[Y] = 0;
	m_end[Z] = 0;
}

/* main function - program entry point */
int main(int argc, char** argv)
{
	glutInit(&argc, argv);		//starts up GLUT
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);

	glutInitWindowSize(900, 600);
	glutInitWindowPosition(100, 50);

	glutCreateWindow("Project");	//creates the window

	//display callback
	glutDisplayFunc(display);

	//keyboard callback
	glutKeyboardFunc(keyboard);
	glutSpecialFunc(special);

	//mouse callbacks
	glutMouseFunc(mouse);
	glutMotionFunc(mouseMotion);
	glutPassiveMotionFunc(mousePassiveMotion);

	//resize callback
	glutReshapeFunc(reshape);

	//fps timer callback
	glutTimerFunc(17, FPSTimer, 0);

	init();

	createOurMenu();

	glutMainLoop();				//starts the event glutMainLoop
	return(0);					//return may not be necessary on all compilers
}
