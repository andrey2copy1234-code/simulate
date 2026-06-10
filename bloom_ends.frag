#version 120

uniform sampler2D u_base; 
uniform sampler2D u_glow; 

void main() {
    // ИСПРАВЛЕНО: Явное указание индекса массива [0] и замена .xy на .st для стабильности
    vec2 uv = gl_TexCoord[0].st;
    
    // Переворачиваем Y-координату только для RenderTexture блума
    vec2 uv_glow = vec2(uv.x, uv.y);

    vec4 base = texture2D(u_base, uv);
    vec4 glow = texture2D(u_glow, uv_glow);

    vec3 hdrColor = base.rgb + glow.rgb;

    // Сжатие Рейнхарда (Tone Mapping)
    vec3 mappedColor = hdrColor / (hdrColor + vec3(1.0));

    gl_FragColor = vec4(mappedColor, 1.0);
}
