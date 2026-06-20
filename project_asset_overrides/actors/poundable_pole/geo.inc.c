// 0x0D0000B8
const GeoLayout wooden_post_geo[] = {
   GEO_SHADOW(SHADOW_CIRCLE_4_VERTS, 0x96, 120),
   GEO_OPEN_NODE(),
      GEO_CULLING_RADIUS(450),
      GEO_OPEN_NODE(),
         GEO_DISPLAY_LIST(LAYER_OPAQUE, poundable_pole_seg6_dl_06002410),
      GEO_CLOSE_NODE(),
   GEO_CLOSE_NODE(),
   GEO_END(),
};
