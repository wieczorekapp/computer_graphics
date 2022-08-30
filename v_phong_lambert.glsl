#version 330

//Zmienne jednorodne
uniform mat4 P;
uniform mat4 V;
uniform mat4 M;
uniform vec4 lp1;
uniform vec4 lp2;

//Atrybuty
in vec4 vertex; //wspolrzedne wierzcholka w przestrzeni modelu
in vec4 normal; //wektor normalny w przestrzeni modelu
in vec2 texCoord0; //współrzędne teksturowania


//Zmienne interpolowane
out vec4 l;
out vec4 l2;

out vec4 n;
out vec4 v;


out vec2 iTexCoord0; //interpolowane współrzędne tekstruwanie przekazywane do FS

void main(void) {
    // pobranie położenia 2 źródeł światła
    vec4 lp = lp1; //pozcyja światła, przestrzeń świata
    vec4 lp2 = lp2; //pozcyja światła, przestrzeń świata

    l = normalize(V * lp - V*M*vertex); //wektor do światła w przestrzeni oka
    v = normalize(vec4(0, 0, 0, 1) - V * M * vertex); //wektor do obserwatora w przestrzeni oka
    n = normalize(V * M * normal); //wektor normalny w przestrzeni oka

    // wektor do 2 źródła światła
    l2 = normalize(V * lp2 - V*M*vertex); //wektor do światła w przestrzeni oka dla 2 źródła światła
    
    // przekazanie współrzędnych teksturowania
    iTexCoord0 = texCoord0;
    
    gl_Position=P*V*M*vertex;
}
