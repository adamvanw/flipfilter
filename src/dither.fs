#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

out vec4 finalColor;

vec3 rgb2hsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

bool ninthOpacity(vec2 coord) {
    float mx = mod(coord.x, 3.0);
    float my = mod(coord.y, 3.0);
    return (mx >= 1.0 && mx < 2.0) && (my >= 1.0 && my < 2.0);
}

bool fourthOpacity(vec2 coord) {
    float mx = mod(coord.x, 2.0);
    float my = mod(coord.y, 2.0);
    return (mx >= 1.0) && (my >= 1.0);
}

bool halfOpacity(vec2 coord) {
    float sum = mod(coord.x, 2.0) + mod(coord.y, 2.0);
    return mod(sum, 2.0) >= 1.0;
}

bool threeFourthsOpacity(vec2 coord) {
    float mx = mod(coord.x, 2.0);
    float my = mod(coord.y, 2.0);
    return !((mx < 1.0) && (my < 1.0));  // Paint all except bottom-left
}

bool eightNinthsOpacity(vec2 coord) {
    float mx = mod(coord.x, 3.0);
    float my = mod(coord.y, 3.0);
    return !((mx < 1.0) && (my < 1.0));  // Skip only the center pixel
}

uniform vec4 lightColor;  // Color for "painted" pixels
uniform vec4 darkColor;   // Color for "unpainted" pixels

void main() {
    vec4 texColor = texture(texture0, fragTexCoord);
    vec3 hsv = rgb2hsv(texColor.rgb);
    float v = hsv.z;
    
    // Use screen-space coordinates instead of texture coordinates
    vec2 pixelCoord = gl_FragCoord.xy;
    bool paintPixel = false;
    
    if (v <= 1.0/12.0) {
        paintPixel = false;
    } else if (v <= 1.0/6.0) {
        paintPixel = ninthOpacity(pixelCoord);
    } else if (v <= 1.0/3.0) {
        paintPixel = fourthOpacity(pixelCoord);
    } else if (v <= 2.0/3.0) {
        paintPixel = halfOpacity(pixelCoord);
    } else if (v <= 6.0/7.0) {
        paintPixel = threeFourthsOpacity(pixelCoord);
    } else if (v <= 10.0/11.0) {
        paintPixel = eightNinthsOpacity(pixelCoord);
    } else {
        paintPixel = true;
    }
    
    finalColor = paintPixel ? vec4(lightColor.rgb, texColor.a) : vec4(darkColor.rgb, texColor.a);
}