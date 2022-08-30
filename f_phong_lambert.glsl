#version 330

uniform sampler2D textureMap0;
uniform int mode; // tryb cieniowania

out vec4 pixelColor; //Zmienna wyjsciowa fragment shadera. Zapisuje sie do niej ostateczny (prawie) kolor piksela

in vec4 ic; 
in vec4 n;
in vec4 l;
in vec4 v;

in vec4 l2;

in vec2 iTexCoord0;

void main(void) {

	//Znormalizowane interpolowane wektory(ważne, by ponowanie znormalizwać)
	vec4 ml = normalize(l);
	vec4 ml2 = normalize(l2);

	vec4 mn = normalize(n);
	vec4 mv = normalize(v);


	//Parametry powierzchni
	vec4 kd = texture(textureMap0,iTexCoord0); // kolor światłą rozproszonego z tekstury (Lambert i Phong)

	//Obliczenie modelu oświetlenia(obliczenie wartości kąta pomiędzy wektoram do swaitła i normalnym)
	// x2 bo 2 źródła swiatła
	float nl = clamp(dot(mn, ml), 0, 1);
	float nl2 = clamp(dot(mn, ml2), 0, 1);

	if(mode == 1){
		// Lambert
		pixelColor= vec4(kd.rgb * nl + kd.rgb *nl2, kd.a); // tylko światło rozproszone
	} else {
		//Wektor odbity do Phonga względem 2 źródeł swiatła
		vec4 mr = reflect(-ml, mn);
		vec4 mr2 = reflect(-ml2, mn);

		// parametry powierzchni dodatkowe do Phonga
		vec4 ks = texture(textureMap0,iTexCoord0); // kolor światłą odbitego z tej samej tekstury (Phong)

		float rv = pow(clamp(dot(mr, mv), 0, 1), 50); // alfa = 50
		float rv2 = pow(clamp(dot(mr2, mv), 0, 1), 50); // alfa = 50

		pixelColor= vec4(kd.rgb * nl + kd.rgb *nl2, kd.a) + vec4(ks.rgb*rv + ks.rgb*rv2, 0); // z odbiciem
	}
}
