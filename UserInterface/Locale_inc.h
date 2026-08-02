#pragma once

//////////////////////////////////////////////////////////////////////////
// ### Default Ymir Macros ###
#define LOCALE_SERVICE_EUROPE
#define ENABLE_COSTUME_SYSTEM
#define ENABLE_ENERGY_SYSTEM
#define ENABLE_DRAGON_SOUL_SYSTEM
#define ENABLE_NEW_EQUIPMENT_SYSTEM
// ### Default Ymir Macros ###
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// ### New From LocaleInc ###
#define ENABLE_PACK_GET_CHECK
#define ENABLE_CANSEEHIDDENTHING_FOR_GM
#define ENABLE_PROTOSTRUCT_AUTODETECT
#define ENABLE_PLAYER_PER_ACCOUNT5
#define ENABLE_LEVEL_IN_TRADE
#define ENABLE_DICE_SYSTEM
#define ENABLE_EXTEND_INVEN_SYSTEM
#define ENABLE_LVL115_ARMOR_EFFECT
#define ENABLE_SLOT_WINDOW_EX
#define ENABLE_TEXT_LEVEL_REFRESH
#define ENABLE_USE_COSTUME_ATTR
#define ENABLE_DISCORD_RPC
#define ENABLE_PET_SYSTEM_EX
#define ENABLE_NO_DSS_QUALIFICATION
//#define ENABLE_NO_SELL_PRICE_DIVIDED_BY_5
#define ENABLE_PENDANT_SYSTEM
#define ENABLE_GLOVE_SYSTEM
#define ENABLE_MOVE_CHANNEL
#define ENABLE_QUIVER_SYSTEM
#define ENABLE_RACE_HEIGHT
#define ENABLE_ELEMENTAL_TARGET
#define ENABLE_INGAME_CONSOLE
#define ENABLE_4TH_AFF_SKILL_DESC
#define ENABLE_LOCALE_COMMON
#define ENABLE_GUILD_TOKEN_AUTH
#define ENABLE_DS_GRADE_MYTH
#define ENABLE_CONQUEROR_UI
#define ENABLE_IMGUI_MANAGER


#define ENABLE_NEW_EVENT_STRUCT
#ifdef ENABLE_NEW_EVENT_STRUCT
#define USE_NEW_EVENT_TEXT_AUTO_Y
#endif

#define WJ_SHOW_MOB_INFO
#ifdef WJ_SHOW_MOB_INFO
#define ENABLE_SHOW_MOBAIFLAG
#define ENABLE_SHOW_MOBLEVEL
#define WJ_SHOW_MOB_INFO_EX
#endif
// ### New From LocaleInc ###
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// ### From GameLib ###
#define ENABLE_WOLFMAN_CHARACTER
#ifdef ENABLE_WOLFMAN_CHARACTER
// #define DISABLE_WOLFMAN_ON_CREATE
#endif
// #define ENABLE_MAGIC_REDUCTION_SYSTEM
#define ENABLE_MOUNT_COSTUME_SYSTEM
#define ENABLE_WEAPON_COSTUME_SYSTEM
// ### From GameLib ###
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// ### New System Defines - Extended Version ###

#define ENABLE_ACCE_COSTUME_SYSTEM
#ifdef ENABLE_ACCE_COSTUME_SYSTEM
// #define USE_ACCE_ABSORB_WITH_NO_NEGATIVE_BONUS
#endif

#define ENABLE_MOUSEWHEEL_EVENT

#define ENABLE_HIGHLIGHT_NEW_ITEM

// it shows emojis in the textlines
#define ENABLE_EMOJI_SYSTEM

// effects while hidden won't show up
#define __ENABLE_STEALTH_FIX__

// circle dots in minimap instead of squares
#define ENABLE_MINIMAP_WHITEMARK_CIRCLE
#define ENABLE_MINIMAP_TELEPORT_CLICK // click on minimap as gm to warp directly

// enable the won system as a currency
#define ENABLE_CHEQUE_SYSTEM
#ifdef ENABLE_CHEQUE_SYSTEM
#define DISABLE_CHEQUE_DROP
#define ENABLE_WON_EXCHANGE_WINDOW
#endif

// for debug: print received packets
// #define ENABLE_PRINT_RECV_PACKET_DEBUG

// ### New System Defines - Extended Version ###
//////////////////////////////////////////////////////////////////////////
#define __BL_FOG_FIX__											// Fog fix
#define __BL_GR2_FILE_LOAD_IMPROVE__							// Improve GR2 files loading
#define __BL_CLIP_MASK__										// Official Clip Mask Integration
#define __BL_MOUSE_WHEEL_TOP_WINDOW__							// Official Mouse Wheel Scroll Integration
#define __BL_HIT__												// Official Collision Update
#define __BL_AUTO_LANTERN_EFFECT__								// Lantern Effect Fix
#define ENABLE_QUEST_RENEWAL									// Quest Page Renewal

// ### DIRECTX11 Configuration ###
#define ENABLE_MSAA
#define MSAA_DEFAULT_SAMPLE_COUNT 4

#define ENABLE_BLOOM
#define BLOOM_DEFAULT_THRESHOLD 0.8f
#define BLOOM_DEFAULT_INTENSITY 1.0f

#define ENABLE_GODRAYS

#define ENABLE_SSAO


// Celestial Body (sun/moon disk rendered on skybox)
#define ENABLE_CELESTIAL_BODY

#define ENABLE_HEIGHT_FOG


// D3D11 Feature Level - minimum required
// Maximum constants for shader system
#define MAX_LIGHTS 8

// ### DIRECTX11 Configuration ###

#define ENABLE_FRUSTUM_CULLING

#define ENABLE_CHAR_RENDER_LIMIT
#define CHAR_RENDER_LIMIT_DEFAULT       0        // Max characters rendered (0 = no limit)

// Effect Limit - Limit total active effect instances
#define ENABLE_EFFECT_LIMIT
#define EFFECT_LIMIT_DEFAULT            500      // Max effects total (0 = no limit)
#define EFFECT_LIMIT_DISTANCE_DEFAULT   3000.0f  // Skip far effects when at limit


#define ENABLE_UPDATE_CULLING
#define UPDATE_CULLING_DISTANCE_FAR     15000.0f  // Very far: skip most updates
#define UPDATE_CULLING_DISTANCE_MID     8000.0f   // Mid range: reduce update frequency
#define UPDATE_CULLING_FRAME_SKIP_FAR   8         // Update every N frames when far
#define UPDATE_CULLING_FRAME_SKIP_MID   4         // Update every N frames when mid

#define ENABLE_DEFORM_CULLING

#define ENABLE_ANIMATION_LOD

#define ENABLE_SHADOW_RENDER_LIMIT
#define SHADOW_RENDER_LIMIT_DEFAULT     30       // Max shadow casters (0 = no limit)
#define ANIMATION_LOD_DISTANCE_HIGH     3000.0f   // Full quality animations
#define ANIMATION_LOD_DISTANCE_MID      8000.0f   // Reduced quality
#define ANIMATION_LOD_SKIP_FRAMES_MID   2         // Update every N frames at mid distance
#define ANIMATION_LOD_SKIP_FRAMES_FAR   4         // Update every N frames at far distance

#define ENABLE_RENDER_MODE_GROUPING

#define ENABLE_FIX_MOBS_LAG

// ### RENDERING OPTIMIZATIONS ###
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// ### SHADOW CAMERA CONFIGURATION ###

#define SHADOW_PROJECTION_SIZE          2550.0f

#define SHADOW_LIGHT_OFFSET             1250.0f

// Shadow projection near plane (minimum render distance)
#define SHADOW_NEAR_PLANE               1.0f

#define SHADOW_FAR_PLANE                15000.0f

#define SHADOW_LIGHT_DIR_X              1.732f
#define SHADOW_LIGHT_DIR_Z              (2.0f * 1.732f)

// ### SHADOW CAMERA CONFIGURATION ###
//////////////////////////////////////////////////////////////////////////


// Shadow map coverage sizes (world units)
#define SHADOW_BIG_MAP_SIZE             5000.0f
#define SHADOW_LOCAL_MAP_SIZE           600.0f

#define SHADOW_OPACITY_DEFAULT          80.0f

// ### DUAL SHADOW MAP CONFIGURATION ###
//////////////////////////////////////////////////////////////////////////

//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
