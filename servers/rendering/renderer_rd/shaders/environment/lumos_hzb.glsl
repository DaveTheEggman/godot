#[compute]

#version 450

#VERSION_DEFINES

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler point_sampler;
layout(set = 0, binding = 1) uniform texture2D src_texture;
layout(set = 0, binding = 2, r32f) uniform writeonly image2D dst_mip;

layout(push_constant, std430) uniform Params {
	ivec2 src_size;
	ivec2 dst_size;
}
params;

void main() {
	ivec2 dst_pos = ivec2(gl_GlobalInvocationID.xy);
	if (dst_pos.x >= params.dst_size.x || dst_pos.y >= params.dst_size.y) {
		return;
	}

#ifdef MODE_INITIALIZE
	// We convert the format here so later mips can be written as storage images
	float depth = texelFetch(sampler2D(src_texture, point_sampler), dst_pos, 0).r;
#endif

#ifdef MODE_REDUCE
	// Basic 2x2 downsample. Grabbing the farthest pixel (max depth)
	// so our screen-space rays don't just clip through everything.
	// Note: godot defaults to standard Z (0=near, 1=far).
	// TODO: if we ever go reversed-Z, swap max() to min() or this whole thing bricks
	ivec2 src_pos = dst_pos * 2;
	ivec2 src_clamp = params.src_size - ivec2(1);

	float d0 = texelFetch(sampler2D(src_texture, point_sampler), min(src_pos + ivec2(0, 0), src_clamp), 0).r;
	float d1 = texelFetch(sampler2D(src_texture, point_sampler), min(src_pos + ivec2(1, 0), src_clamp), 0).r;
	float d2 = texelFetch(sampler2D(src_texture, point_sampler), min(src_pos + ivec2(0, 1), src_clamp), 0).r;
	float d3 = texelFetch(sampler2D(src_texture, point_sampler), min(src_pos + ivec2(1, 1), src_clamp), 0).r;

	float depth = max(max(d0, d1), max(d2, d3));
#endif

	imageStore(dst_mip, dst_pos, vec4(depth, 0.0, 0.0, 0.0));
}
