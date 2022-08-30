/*
Niniejszy program jest wolnym oprogramowaniem; możesz go
rozprowadzać dalej i / lub modyfikować na warunkach Powszechnej
Licencji Publicznej GNU, wydanej przez Fundację Wolnego
Oprogramowania - według wersji 2 tej Licencji lub(według twojego
wyboru) którejś z późniejszych wersji.

Niniejszy program rozpowszechniany jest z nadzieją, iż będzie on
użyteczny - jednak BEZ JAKIEJKOLWIEK GWARANCJI, nawet domyślnej
gwarancji PRZYDATNOŚCI HANDLOWEJ albo PRZYDATNOŚCI DO OKREŚLONYCH
ZASTOSOWAŃ.W celu uzyskania bliższych informacji sięgnij do
Powszechnej Licencji Publicznej GNU.

Z pewnością wraz z niniejszym programem otrzymałeś też egzemplarz
Powszechnej Licencji Publicznej GNU(GNU General Public License);
jeśli nie - napisz do Free Software Foundation, Inc., 59 Temple
Place, Fifth Floor, Boston, MA  02110 - 1301  USA
*/

#define GLM_FORCE_RADIANS
#define GLM_FORCE_SWIZZLE
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdlib.h>
#include <stdio.h>
#include "constants.h"
#include "lodepng.h"
#include "shaderprogram.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <iostream>

// tryb dla sfery, odwraca normalne 1=> odwrócone 0 => orginalne
// wykorzystane przy sferze
int invert_mode_norms;


//=============================================================================================================================
// klasa importująca modele z plików
class loadObj3D {
public:
	// pola w obiekcie importu modelu 
	std::vector<glm::vec4> verts;
	std::vector<glm::vec4> norms;
	std::vector<glm::vec2> textureCoords;
	std::vector<unsigned int> indices;

	//konstruktor z wczytaniem danego obiektu
	loadObj3D(std::string plik) {
		loadModel(plik);
	}

	//dekonstrutor
	~loadObj3D() {}

	// wczytanie obiektu przy pomocy assimp
	void loadModel(std::string plik) {
		Assimp::Importer importer;

		// aiProcess_Triangulate - modele jest złożony z trójkątów
		// aiProcess_FlipUVs - odwrócenie współrzędnych teksturowania na zgodne z OpenGL
		// aiProcess_Gen[Smooth]Normals - jeżeli brak wylicza normalne do ściany, lub jeżli opcjonale smooth do wierzchołka
		const aiScene* scene = importer.ReadFile(plik,
			aiProcess_Triangulate |
			aiProcess_FlipUVs |
			aiProcess_GenSmoothNormals |
			aiProcess_RemoveRedundantMaterials |
			aiProcess_OptimizeGraph |
			aiProcess_OptimizeMeshes |
			aiProcess_SplitLargeMeshes |
			aiProcess_JoinIdenticalVertices |
			aiProcess_ImproveCacheLocality
		);
		std::cout << importer.GetErrorString() << std::endl;

		aiMesh* mesh = scene->mMeshes[0];
		for (int i = 0; i < mesh->mNumVertices; i++) {
			aiVector3D vertex = mesh->mVertices[i];
			this->verts.push_back(glm::vec4(vertex.x, vertex.y, vertex.z, 1));
			//std::cout << "Vertex" << std::endl;
			//std::cout << this->verts.back().x << " " << this->verts.back().y << " " << this->verts.back().z << std::endl;

			aiVector3D normal = mesh->mNormals[i];
			if (invert_mode_norms == 1) {
				this->norms.push_back(glm::vec4(-1* normal.x, -1 * normal.y, -1 * normal.z, 0));
			}
			else {
				this->norms.push_back(glm::vec4(normal.x, normal.y, normal.z, 0));
			}
			//std::cout << "Normal" << std::endl;
			//std::cout << normal.x << " " << normal.y << " " << normal.z << std::endl;


			aiVector3D texCoord = mesh->mTextureCoords[0][i];
			this->textureCoords.push_back(glm::vec2(texCoord.x, texCoord.y));
			//std::cout << "TexCoord" << std::endl;
			//std::cout << texCoord.x << " " << texCoord.y << std::endl;
		}

		for (int i = 0; i < mesh->mNumFaces; i++) {
			aiFace& face = mesh->mFaces[i];

			for (int j = 0; j < face.mNumIndices; j++) {
				this->indices.push_back(face.mIndices[j]);
			}
		}

		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

		for (int i = 0; i < material->GetTextureCount(aiTextureType_DIFFUSE); i++) {
			aiString str;
			aiTextureMapping mapping;
			unsigned int uvMapping;
			ai_real blend;
			aiTextureOp op;
			aiTextureMapMode mapMode;
			material->GetTexture(aiTextureType_DIFFUSE, i, &str);
		}
	}

};

//=============================================================================================================================
// funkcja wczytująca tekstury z plików
GLuint readTexture(const char* filename) {
	GLuint tex;

	glActiveTexture(GL_TEXTURE0);


	//Wczytanie do pamięci komputera
	std::vector<unsigned char> image;   //Alokuj wektor do wczytania obrazka
	unsigned width, height;   //Zmienne do których wczytamy wymiary obrazka
	//Wczytaj obrazek
	unsigned error = lodepng::decode(image, width, height, filename);

	//Import do pamięci karty graficznej
	glGenTextures(1, &tex); //Zainicjuj jeden uchwyt
	glBindTexture(GL_TEXTURE_2D, tex); //Uaktywnij uchwyt
	//Wczytaj obrazek do pamięci KG skojarzonej z uchwytem
	glTexImage2D(GL_TEXTURE_2D, 0, 4, width, height, 0,
		GL_RGBA, GL_UNSIGNED_BYTE, (unsigned char*)image.data());

	//różne paramtry związane z pobieraniem koloru z tekstury, tu ustawinie bipolar filtering
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	return tex;
}

//======================================================================================================================
// ZMIENNE GLOBALNE KONIEC =============================================================================================
//======================================================================================================================

//kamera mode 1 => odblokowana; 2 => zablokowana
int camera_mode = 1;

// poruszanie kamerą
float speed_x = 0; //[radiany/s]
float speed_y = 0; //[radiany/s]
float walk_speed = 0;
float multipler = 8; // przyspieszenie poruszania i obracania

// pozycja poczatkowa (zawiszone dowolnie)
//glm::vec3 pos = glm::vec3(0, 5, -15);

// położenie początkowe wagonika
float wagonik_x = 0.0f;
float wagonik_y = 0.0f;
float wagonik_z = 0.0f;

// nachylenie wagonika
float rotacja_os_z = 0.0f;
float rotacja_os_y = 0.0f;

// przesunięcie kamery do wagonika, stałe względem początku animacji, potrzebne do śledenia wagonika
float offset_camera_x = -16.0f;
float offset_camera_y = 9.0f;
float offset_camera_z = -9.5f;

// pozycja kamery
float camera_position_x = wagonik_x + offset_camera_x;
float camera_position_y = wagonik_y + offset_camera_y;
float camera_position_z = wagonik_z + offset_camera_z;

glm::vec3 pos = glm::vec3(camera_position_x, camera_position_y, camera_position_z);

//======================================================================================================================
// ZMIENNE GLOBALNE KONIEC =============================================================================================
//======================================================================================================================

glm::vec3 calcDir(float kat_x, float kat_y) {
	glm::vec4 dir = glm::vec4(0, 0, 1, 0);
	glm::mat4 M = glm::rotate(glm::mat4(1.0f), kat_y, glm::vec3(0, 1, 0));
	M = glm::rotate(M, kat_x, glm::vec3(1, 0, 0));
	dir = M * dir;
	return glm::vec3(dir);
}

// globalna zmienna do skalownia okana projektu
float aspectRatio=1;

// uchwyty do programów cieniujących 
ShaderProgram * sp;

// uchwyty do tekstur
GLuint tex0; 
GLuint tex1;
GLuint tex2;
GLuint tex3;
GLuint tex4;

// globalny obiekt do wczytania obiektu
loadObj3D* wagonik;
loadObj3D* tor;
loadObj3D* trawa;
loadObj3D* obelisk;
loadObj3D* niebo;


//Procedura obsługi błędów
void error_callback(int error, const char* description) {
	fputs(description, stderr);
}

// obsługa klaiatury
void keyCallback(GLFWwindow* window,int key,int scancode,int action,int mods) {
    if (action==GLFW_PRESS) {

		if (key == GLFW_KEY_SPACE) {
			if (camera_mode == 1) {
				camera_mode = 0;
				std::cout << "Kamera zablokowana" << std::endl;
			}
			else {
				camera_mode = 1;
				std::cout << "Kamera odblokowana" << std::endl;
			}
		}

		if (camera_mode == 1) {
			if (key == GLFW_KEY_UP) walk_speed = 2 * multipler;
			if (key == GLFW_KEY_DOWN) walk_speed = -2 * multipler;
		}

		if (key == GLFW_KEY_LEFT) speed_y = 1 * multipler / 4;
		if (key == GLFW_KEY_RIGHT) speed_y = -1 * multipler / 4;
		if (key == GLFW_KEY_PAGE_UP) speed_x = 1 * multipler / 4;
		if (key == GLFW_KEY_PAGE_DOWN) speed_x = -1 * multipler / 4;

    }
    if (action==GLFW_RELEASE) {

		if (camera_mode == 1) {
			if (key == GLFW_KEY_UP) walk_speed = 0;
			if (key == GLFW_KEY_DOWN) walk_speed = 0;
		}

		if (key == GLFW_KEY_LEFT) speed_y = 0;
		if (key == GLFW_KEY_RIGHT) speed_y = 0;
		if (key == GLFW_KEY_PAGE_UP) speed_x = 0;
		if (key == GLFW_KEY_PAGE_DOWN) speed_x = 0;
    }
}


// obsługa skalowania rozmiaru okna
void windowResizeCallback(GLFWwindow* window,int width,int height) {
    if (height==0) return;
    aspectRatio=(float)width/(float)height;
    glViewport(0,0,width,height);
}

//Procedura inicjująca
void initOpenGLProgram(GLFWwindow* window) {
	//************Tutaj umieszczaj kod, który należy wykonać raz, na początku programu************
	glClearColor(0.51, 0.69, 0.84, 1); // zdefiniowanie koloru czyszczenie, tła 
	glEnable(GL_DEPTH_TEST); // uruchominie metody z-bufora do ustalenia widocznych elementów(pomiar oddalenia obiektu na scenie)
	glfwSetWindowSizeCallback(window,windowResizeCallback);
	glfwSetKeyCallback(window,keyCallback);

	//0 - Phong 1 - Lambert
	sp =new ShaderProgram("v_phong_lambert.glsl",NULL,"f_phong_lambert.glsl"); // program cieniujący Phonga(kolor tekstury jako kolor odbicia) lub Lambert
	tex0 = readTexture("tor.png");
	tex1 = readTexture("grass2.png");
	tex2 = readTexture("metal.png");
	tex3 = readTexture("obelisk.png");
	tex4 = readTexture("banka.png");

	invert_mode_norms = 0;
	tor = new loadObj3D(std::string("tor.obj"));
	wagonik = new loadObj3D(std::string("wagonik2.obj"));
	trawa = new loadObj3D(std::string("grass.obj"));
	obelisk = new loadObj3D(std::string("obelisk.obj"));


	invert_mode_norms = 1;
	niebo = new loadObj3D(std::string("banka.obj"));

	if (camera_mode == 1) std::cout << "Kamera odblokowana" << std::endl;
	
}


//Zwolnienie zasobów zajętych przez program
void freeOpenGLProgram(GLFWwindow* window) {
    //************Tutaj umieszczaj kod, który należy wykonać po zakończeniu pętli głównej************
	// usunięcie programu cieniującego
	delete sp;

	// usunięci modeli wczytanych z plików obj
	delete tor;
	delete wagonik;
	delete trawa;
	delete obelisk;
	delete niebo;

	// zwolninie tekstru z pamięci
	glDeleteTextures(1, &tex4);
	glDeleteTextures(1, &tex3);
	glDeleteTextures(1, &tex2);
	glDeleteTextures(1, &tex1);
	glDeleteTextures(1, &tex0);
}


//Procedura rysująca zawartość sceny
void drawScene(GLFWwindow* window,float kat_x, float kat_y) {
	//************Tutaj umieszczaj kod rysujący obraz******************l
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // czyszczenie bufora kolorów i z-bufora 

	// macierz widoku działająca albo jak luzna kamera, albo podpięta pod wagonik, zmina spacją
	glm::mat4 V = glm::lookAt(pos, pos + calcDir(kat_x, kat_y), glm::vec3(0.0f, 1.0f, 0.0f));

	// zwiększenie obszaru renderowania obrazu, kąt widzenia jest zależny od rozmiaru okna
    glm::mat4 P=glm::perspective(50.0f*PI/180.0f, aspectRatio, 0.01f, 250.0f);

	sp->use();//Aktywacja programu cieniującego
	//Przeslij parametry programu cieniującego do karty graficznej
	glUniformMatrix4fv(sp->u("P"), 1, false, glm::value_ptr(P));
	glUniformMatrix4fv(sp->u("V"), 1, false, glm::value_ptr(V));

	// Ustalenie położenia 2 źródeł światła i przekazanie jako zmienne jednorodne takie same do wszystkich obiektów
	// lecz mżna je zmienić względem obiektów
	glm::vec4 lp1 = glm::vec4(0, 10, -25, 1);
	glUniform4fv(sp->u("lp1"), 1, glm::value_ptr(lp1));
	glm::vec4 lp2 = glm::vec4(0, 20, 65, 1);
	glUniform4fv(sp->u("lp2"), 1, glm::value_ptr(lp2));

	// KONIEC STAŁEJ KONFIGURACJI
	//=======================================================================================================================================

	// POŁOŻENIE ROLLERCOSTERA ###########################################################
	glm::mat4 M = glm::mat4(1.0f);
	glUniformMatrix4fv(sp->u("M"), 1, false, glm::value_ptr(M));
	glUniform1i(sp->u("mode"),1); // tryb cieniowania 0 => PHONG 1=> LAMBERT


    glEnableVertexAttribArray(sp->a("vertex"));  //Włącz przesyłanie danych do atrybutu vertex
    glVertexAttribPointer(sp->a("vertex"), 4, GL_FLOAT, false, 0, tor->verts.data()); //Wskaż tablicę z danymi dla atrybutu vertex

	glEnableVertexAttribArray(sp->a("normal"));  //Włącz przesyłanie danych do atrybutu normal
	glVertexAttribPointer(sp->a("normal"), 4, GL_FLOAT, false, 0, tor->norms.data()); //Wskaż tablicę z danymi dla atrybutu normal

	glEnableVertexAttribArray(sp->a("texCoord0"));  //Włącz przesyłanie danych do atrybutu texCoord
	glVertexAttribPointer(sp->a("texCoord0"), 2, GL_FLOAT, false, 0, tor->textureCoords.data()); //Wskaż tablicę z danymi dla atrybutu texCoord

	glUniform1i(sp->u("textureMap0"), 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex0);

	glDrawElements(GL_TRIANGLES, tor->indices.size(), GL_UNSIGNED_INT, tor->indices.data());


	// POŁOŻENIE WAGONIKA ###########################################################
	glm::mat4 M2 = glm::mat4(1.0f);
	wagonik_x += 0.01f;
	wagonik_y += 0.001f;
	wagonik_z += 0.001f;

	//// rotowanie wagonika(kąt odchylenia do góry, w dół i poziomi obrót(wagonik nie będzie miał rotacji skośnej))
	//rotacja_os_z += 0.0001f;
	//M2 = glm::rotate(M2, rotacja_os_z, glm::vec3(0.0f, 0.0f, 1.0f));

	//rotacja_os_y += 0.0001f;
	//M2 = glm::rotate(M2, rotacja_os_y, glm::vec3(0.0f, 1.0f, 0.0f));

	// zminan położenie po zrotowaniu
	M2 = glm::translate(M2, glm::vec3(wagonik_x, wagonik_y, wagonik_z));

	glUniformMatrix4fv(sp->u("M"), 1, false, glm::value_ptr(M2));
	glUniform1i(sp->u("mode"), 1); // tryb cieniowania 0 => PHONG 1=> LAMBERT

	glEnableVertexAttribArray(sp->a("vertex"));  //Włącz przesyłanie danych do atrybutu vertex
	glVertexAttribPointer(sp->a("vertex"), 4, GL_FLOAT, false, 0, wagonik->verts.data()); //Wskaż tablicę z danymi dla atrybutu vertex

	glEnableVertexAttribArray(sp->a("normal"));  //Włącz przesyłanie danych do atrybutu normal
	glVertexAttribPointer(sp->a("normal"), 4, GL_FLOAT, false, 0, wagonik->norms.data()); //Wskaż tablicę z danymi dla atrybutu normal

	glEnableVertexAttribArray(sp->a("texCoord0"));  //Włącz przesyłanie danych do atrybutu texCoord
	glVertexAttribPointer(sp->a("texCoord0"), 2, GL_FLOAT, false, 0, wagonik->textureCoords.data()); //Wskaż tablicę z danymi dla atrybutu texCoord

	glUniform1i(sp->u("textureMap0"), 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex2);

	glDrawElements(GL_TRIANGLES, wagonik->indices.size(), GL_UNSIGNED_INT, wagonik->indices.data());


	// POŁOŻENIE PODŁOŻA ###########################################################
	glm::mat4 M3 = glm::mat4(1.0f);
	//M3 = glm::translate(M3, glm::vec3(0.0f, 0.0f, 0.0f));

	glUniformMatrix4fv(sp->u("M"), 1, false, glm::value_ptr(M3));
	glUniform1i(sp->u("mode"), 1); // tryb cieniowania 0 => PHONG 1=> LAMBERT

	glEnableVertexAttribArray(sp->a("vertex"));  //Włącz przesyłanie danych do atrybutu vertex
	glVertexAttribPointer(sp->a("vertex"), 4, GL_FLOAT, false, 0, trawa->verts.data()); //Wskaż tablicę z danymi dla atrybutu vertex

	glEnableVertexAttribArray(sp->a("normal"));  //Włącz przesyłanie danych do atrybutu normal
	glVertexAttribPointer(sp->a("normal"), 4, GL_FLOAT, false, 0, trawa->norms.data()); //Wskaż tablicę z danymi dla atrybutu normal

	glEnableVertexAttribArray(sp->a("texCoord0"));  //Włącz przesyłanie danych do atrybutu texCoord
	glVertexAttribPointer(sp->a("texCoord0"), 2, GL_FLOAT, false, 0, trawa->textureCoords.data()); //Wskaż tablicę z danymi dla atrybutu texCoord

	glUniform1i(sp->u("textureMap0"), 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex1);

	glDrawElements(GL_TRIANGLES, trawa->indices.size(), GL_UNSIGNED_INT, trawa->indices.data());

	
	// POŁOŻENIE OBELISKU ###########################################################
	glm::mat4 M4 = glm::mat4(1.0f);
	M4 = glm::translate(M4, glm::vec3(20.0f, 0.0f, 10.0f));

	glUniformMatrix4fv(sp->u("M"), 1, false, glm::value_ptr(M4));
	glUniform1i(sp->u("mode"), 0); // tryb cieniowania 0 => PHONG 1=> LAMBERT

	glEnableVertexAttribArray(sp->a("vertex"));  //Włącz przesyłanie danych do atrybutu vertex
	glVertexAttribPointer(sp->a("vertex"), 4, GL_FLOAT, false, 0, obelisk->verts.data()); //Wskaż tablicę z danymi dla atrybutu vertex

	glEnableVertexAttribArray(sp->a("normal"));  //Włącz przesyłanie danych do atrybutu normal
	glVertexAttribPointer(sp->a("normal"), 4, GL_FLOAT, false, 0, obelisk->norms.data()); //Wskaż tablicę z danymi dla atrybutu normal

	glEnableVertexAttribArray(sp->a("texCoord0"));  //Włącz przesyłanie danych do atrybutu texCoord
	glVertexAttribPointer(sp->a("texCoord0"), 2, GL_FLOAT, false, 0, obelisk->textureCoords.data()); //Wskaż tablicę z danymi dla atrybutu texCoord

	glUniform1i(sp->u("textureMap0"), 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex3);

	glDrawElements(GL_TRIANGLES, obelisk->indices.size(), GL_UNSIGNED_INT, obelisk->indices.data());


	// POŁOŻENIE NIEBA ###########################################################
	glm::mat4 M5 = glm::mat4(1.0f);


	glUniformMatrix4fv(sp->u("M"), 1, false, glm::value_ptr(M5));
	glUniform1i(sp->u("mode"), 1); // tryb cieniowania 0 => PHONG 1=> LAMBERT

	glEnableVertexAttribArray(sp->a("vertex"));  //Włącz przesyłanie danych do atrybutu vertex
	glVertexAttribPointer(sp->a("vertex"), 4, GL_FLOAT, false, 0, niebo->verts.data()); //Wskaż tablicę z danymi dla atrybutu vertex

	glEnableVertexAttribArray(sp->a("normal"));  //Włącz przesyłanie danych do atrybutu normal
	glVertexAttribPointer(sp->a("normal"), 4, GL_FLOAT, false, 0, niebo->norms.data()); //Wskaż tablicę z danymi dla atrybutu normal

	glEnableVertexAttribArray(sp->a("texCoord0"));  //Włącz przesyłanie danych do atrybutu texCoord
	glVertexAttribPointer(sp->a("texCoord0"), 2, GL_FLOAT, false, 0, niebo->textureCoords.data()); //Wskaż tablicę z danymi dla atrybutu texCoord

	glUniform1i(sp->u("textureMap0"), 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex4);

	glDrawElements(GL_TRIANGLES, niebo->indices.size(), GL_UNSIGNED_INT, niebo->indices.data());

	// =============================================================================================================================================================================
	// KONIEC RYSOWANIA ####################################################################################################
	// Wyłączenie przekazywania argumentów do programu cieniującego
    glDisableVertexAttribArray(sp->a("vertex"));  //Wyłącz przesyłanie danych do atrybutu vertex
	glDisableVertexAttribArray(sp->a("normal"));  //Wyłącz przesyłanie danych do atrybutu normal
	glDisableVertexAttribArray(sp->a("texCoord0"));  //Wyłącz przesyłanie danych do atrybutu normal

    glfwSwapBuffers(window); //Przerzuć tylny bufor na przedni(podwójne buforowanie)
}


int main(void)
{
	GLFWwindow* window; //Wskaźnik na obiekt reprezentujący okno

	glfwSetErrorCallback(error_callback);//Zarejestruj procedurę obsługi błędów

	if (!glfwInit()) { //Zainicjuj bibliotekę GLFW
		fprintf(stderr, "Nie można zainicjować GLFW.\n");
		exit(EXIT_FAILURE);
	}

	window = glfwCreateWindow(1000, 1000, "Roller Coster", NULL, NULL); 

	if (!window) //Jeżeli okna nie udało się utworzyć, to zamknij program
	{
		fprintf(stderr, "Nie można utworzyć okna.\n");
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	glfwMakeContextCurrent(window); //Od tego momentu kontekst okna staje się aktywny i polecenia OpenGL będą dotyczyć właśnie jego.
	glfwSwapInterval(1); //Czekaj na 1 powrót plamki przed pokazaniem ukrytego bufora

	if (glewInit() != GLEW_OK) { //Zainicjuj bibliotekę GLEW
		fprintf(stderr, "Nie można zainicjować GLEW.\n");
		exit(EXIT_FAILURE);
	}

	initOpenGLProgram(window); //Operacje inicjujące

	//Główna pętla programu
	// kąty związane z obrotem obserwatora/kamery
	float kat_x = 0;
	float kat_y = 0;

	glfwSetTime(0); //Zeruj timer
	while (!glfwWindowShouldClose(window)) //Tak długo jak okno nie powinno zostać zamknięte
	{
		kat_x += speed_x * glfwGetTime();
		kat_y += speed_y * glfwGetTime();

		// położenie kamery w trybie odblokowanym
		if (camera_mode == 1) {
			pos += (float)(walk_speed * glfwGetTime()) * calcDir(kat_x, kat_y);
			
		}
		else {
			// położenie kamery w trybie zablokowanym, względem położenia wagonika
			camera_position_x = wagonik_x + offset_camera_x;
			camera_position_y = wagonik_y + offset_camera_y;
			camera_position_z = wagonik_z + offset_camera_z;

			pos = glm::vec3(camera_position_x, camera_position_y, camera_position_z);
		}

        glfwSetTime(0); //Zeruj timer
		drawScene(window,kat_x,kat_y); //Wykonaj procedurę rysującą
		glfwPollEvents(); //Wykonaj procedury callback w zalezności od zdarzeń jakie zaszły.
	}

	freeOpenGLProgram(window);

	glfwDestroyWindow(window); //Usuń kontekst OpenGL i okno
	glfwTerminate(); //Zwolnij zasoby zajęte przez GLFW
	exit(EXIT_SUCCESS);
}
