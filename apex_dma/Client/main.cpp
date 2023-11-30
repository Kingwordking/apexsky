#include "main.h"
#include "../Game.h"
#include "imgui.h"
#include "overlay.h"
#include <cstdio>
#include <map>
#include <random>
#include <thread>
typedef Vector D3DXVECTOR3;

typedef uint8_t *PBYTE;
typedef uint8_t BYTE;
typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef WORD *PWORD;

uint32_t check = 0xABCD;

// Left and Right Aim key toggle
extern bool active; // sync
bool ready = false;
extern visuals v;
extern bool aiming;               // read sync
extern uint64_t g_Base;           // write sync

// tdm check
extern int EntTeam; // sync
extern int LocTeam; // sync

extern settings_t global_settings;

extern float bulletspeed; // sync
extern float bulletgrav;  // sync
float veltest = 1.00;     // sync

// Full Map Radar
extern bool mainradartoggle; // Toggle for Main Map radar
bool kingscanyon = false;    // Set for map, ONLY ONE THO
bool stormpoint = true;      // Set for map, ONLY ONE THO

// Map number
extern int map;

extern int spectators;        // write sync
extern int allied_spectators; // write sync
extern bool valid;            // write sync
extern bool next2;            // read write sync

Vector aim_target = Vector(0, 0, 0);

extern bool overlay_t;

extern player players[100];
extern Matrix view_matrix_data;

// Radar Code
#define M_PI 3.14159265358979323846 // matches value in gcc v2 math.h

static D3DXVECTOR3 RotatePoint(D3DXVECTOR3 EntityPos,
                               D3DXVECTOR3 LocalPlayerPos, int posX, int posY,
                               int sizeX, int sizeY, float angle, float zoom,
                               bool *viewCheck) {
  float r_1, r_2;
  float x_1, y_1;

  r_1 = -(EntityPos.y - LocalPlayerPos.y);
  r_2 = EntityPos.x - LocalPlayerPos.x;
  float Yaw = angle - 90.0f;

  float yawToRadian = Yaw * (float)(M_PI / 180.0F);
  x_1 = (float)(r_2 * (float)cos((double)(yawToRadian)) -
                r_1 * sin((double)(yawToRadian))) /
        20;
  y_1 = (float)(r_2 * (float)sin((double)(yawToRadian)) +
                r_1 * cos((double)(yawToRadian))) /
        20;

  *viewCheck = y_1 < 0;

  x_1 *= zoom;
  y_1 *= zoom;

  int sizX = sizeX / 2;
  int sizY = sizeY / 2;

  x_1 += sizX;
  y_1 += sizY;

  if (x_1 < 5)
    x_1 = 5;

  if (x_1 > sizeX - 5)
    x_1 = sizeX - 5;

  if (y_1 < 5)
    y_1 = 5;

  if (y_1 > sizeY - 5)
    y_1 = sizeY - 5;

  x_1 += posX;
  y_1 += posY;

  return D3DXVECTOR3(x_1, y_1, 0);
}
struct RGBA2 {
  int R;
  int G;
  int B;
  int A;
};
std::map<int, RGBA2> teamColors;
// Main Map Radar Color
typedef struct {
  DWORD R;
  DWORD G;
  DWORD B;
  DWORD A;
} RGBA;

typedef RGBA D3DXCOLOR;

// static void FilledRectangle(int x, int y, int w, int h, RGBA color)
//{
//	ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y
//+ h), ImGui::ColorConvertFloat4ToU32(ImVec4(color.R / 255.0, color.G / 255.0,
// color.B / 255.0, color.A / 255.0)), 0, 0);
// }

// Color Team Radar Test. oh god why... This is stupid.. dont do this.. it works
// tho
static void TeamN(int x, int y, int w, int h, RGBA color) {
  ImGui::GetWindowDrawList()->AddRectFilled(
      ImVec2(x, y), ImVec2(x + w, y + h),
      ImGui::ColorConvertFloat4ToU32(ImVec4(color.R / 255.0, color.G / 255.0,
                                            color.B / 255.0, color.A / 255.0)),
      0, 0);
}

static void TeamMiniMap(int x, int y, int radius, int team_id,
                        float targetyaw) {
  RGBA2 color;
  auto it = teamColors.find(team_id);
  if (it == teamColors.end()) {
    // Define the minimum sum of RGB values for a color to be considered "light"
    const int MIN_SUM_RGB = 500;

    // Generate a new random color for this team, discarding colors with a low
    // sum of RGB values
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    RGBA2 color;
    do {
      color = {dis(gen), dis(gen), dis(gen), 255};
    } while (color.R + color.G + color.B < MIN_SUM_RGB);

    // Store the color in the teamColors map
    teamColors[team_id] = color;
  } else {
    // Use the previously generated color for this team
    color = it->second;
  }

  auto colOutline = ImGui::ColorConvertFloat4ToU32(ImVec4(0.0, 0.0, 0.0, 1.0));
  ImVec2 center(x, y);
  ImGui::GetWindowDrawList()->AddCircleFilled(
      center, radius,
      ImGui::ColorConvertFloat4ToU32(ImVec4(color.R / 255.0, color.G / 255.0,
                                            color.B / 255.0, color.A / 255.0)));
  ImGui::GetWindowDrawList()->AddCircle(center, radius, colOutline, 12,
                                        global_settings.mini_map_radar_dot_size2);

  // Draw a line pointing in the direction of each player's aim
  const int numPlayers = 3;
  for (int i = 0; i < numPlayers; i++) {
    float angle = (360.0 - targetyaw) *
                  (M_PI / 180.0); // Replace this with the actual yaw of the
                                  // player, then convert it to radians.
    ImVec2 endpoint(center.x + radius * cos(angle),
                    center.y + radius * sin(angle));
    ImGui::GetWindowDrawList()->AddLine(center, endpoint, colOutline);
  }
}

bool menu = true;
bool firstS = true;
// Radar Settings.. ToDO: Put in ImGui menu to change in game
namespace RadarSettings {
bool Radar = true;
bool teamRadar = true;
bool enemyRadar = true;
int xAxis_Radar = 0;
int yAxis_Radar = 400;
int radartype = 0;
int width_Radar = 400;
int height_Radar = 400;
int distance_Radar = 250;
int distance_Radar2 = 1000;
}; // namespace RadarSettings

void DrawRadarPointMiniMap(D3DXVECTOR3 EneamyPos, D3DXVECTOR3 LocalPos,
                           float LocalPlayerY, float eneamyDist, int team_id,
                           int xAxis, int yAxis, int width, int height,
                           D3DXCOLOR color, float targetyaw) {
  D3DXVECTOR3 siz;
  siz.x = width;
  siz.y = height;
  D3DXVECTOR3 pos;
  pos.x = xAxis;
  pos.y = yAxis;
  bool ck = false;
  D3DXVECTOR3 single = RotatePoint(EneamyPos, LocalPos, pos.x, pos.y, siz.x,
                                   siz.y, LocalPlayerY, 0.3f, &ck);
  if (eneamyDist >= 0.f && eneamyDist < RadarSettings::distance_Radar) {
    for (int i = 1; i <= 30; i++) {
      TeamMiniMap(single.x, single.y, global_settings.mini_map_radar_dot_size1, team_id, targetyaw);
    }
  }
}

void draw_team_point(int pos_x, int pos_y, int team_id) {
  if (team_id == 1) {
    TeamN(pos_x, pos_y, global_settings.main_map_radar_dot_size1, global_settings.main_map_radar_dot_size2,
          {255, 255, 255, 255});
  }
  if (team_id == 2) {
    TeamN(pos_x, pos_y, global_settings.main_map_radar_dot_size1, global_settings.main_map_radar_dot_size2,
          {242, 86, 38, 255});
  }
  if (team_id == 3) {
    TeamN(pos_x, pos_y, global_settings.main_map_radar_dot_size1, global_settings.main_map_radar_dot_size2,
          {242, 86, 38, 255});
  }
  if (team_id == 4) {
    TeamN(pos_x, pos_y, global_settings.main_map_radar_dot_size1, global_settings.main_map_radar_dot_size2,
          {174, 247, 89, 255});
  }
  if (team_id == 5) {
    TeamN(pos_x, pos_y, global_settings.main_map_radar_dot_size1, global_settings.main_map_radar_dot_size2,
          {102, 214, 173, 255});
  }
  if (team_id == 6) {
    TeamN(pos_x, pos_y, global_settings.main_map_radar_dot_size1, global_settings.main_map_radar_dot_size2,
          {98, 244, 234, 255});
  }
  if (team_id == 7) {
    TeamN(pos_x, pos_y, global_settings.main_map_radar_dot_size1, global_settings.main_map_radar_dot_size2,
          {92, 208, 250, 255});
  }
  if (team_id == 8) {
    TeamN(pos_x, pos_y, global_settings.main_map_radar_dot_size1, global_settings.main_map_radar_dot_size2,
          {93, 137, 238, 255});
  }
  if (team_id == 9) {
    TeamN(pos_x, pos_y, global_settings.main_map_radar_dot_size1, global_settings.main_map_radar_dot_size2,
          {164, 105, 252, 255});
  }
  if (team_id == 10) {
    TeamN(pos_x, pos_y, global_settings.main_map_radar_dot_size1, global_settings.main_map_radar_dot_size2,
          {243, 98, 161, 255});
  }
  if (team_id == 11) {
    TeamN(pos_x, pos_y, global_settings.main_map_radar_dot_size1, global_settings.main_map_radar_dot_size2,
          {214, 67, 67, 255});
  }
  if (team_id == 12) {
    TeamN(pos_x, pos_y, global_settings.main_map_radar_dot_size1, global_settings.main_map_radar_dot_size2,
          {230, 116, 51, 255});
  }
  if (team_id == 13) {
    TeamN(pos_x, pos_y, global_settings.main_map_radar_dot_size1, global_settings.main_map_radar_dot_size2,
          {185, 179, 167, 255});
  }
  if (team_id == 14) {
    TeamN(pos_x, pos_y, global_settings.main_map_radar_dot_size1, global_settings.main_map_radar_dot_size2,
          {148, 200, 65, 255});
  }
  if (team_id == 15) {
    TeamN(pos_x, pos_y, global_settings.main_map_radar_dot_size1, global_settings.main_map_radar_dot_size2,
          {86, 174, 91, 255});
  }
  if (team_id == 16) {
    TeamN(pos_x, pos_y, global_settings.main_map_radar_dot_size1, global_settings.main_map_radar_dot_size2,
          {55, 188, 200, 255});
  }
  if (team_id == 17) {
    TeamN(pos_x, pos_y, global_settings.main_map_radar_dot_size1, global_settings.main_map_radar_dot_size2,
          {84, 169, 212, 255});
  }
  if (team_id == 18) {
    TeamN(pos_x, pos_y, global_settings.main_map_radar_dot_size1, global_settings.main_map_radar_dot_size2,
          {98, 121, 203, 255});
  }
  if (team_id == 19) {
    TeamN(pos_x, pos_y, global_settings.main_map_radar_dot_size1, global_settings.main_map_radar_dot_size2,
          {102, 61, 174, 255});
  }
  if (team_id >= 20 && team_id < 36) {
    TeamN(pos_x, pos_y, global_settings.main_map_radar_dot_size1, global_settings.main_map_radar_dot_size2,
          {218, 73, 145, 255});
  }
}

void DrawRadarPoint(D3DXVECTOR3 EneamyPos, D3DXVECTOR3 LocalPos,
                    float LocalPlayerY, float eneamyDist, int team_id,
                    int xAxis, int yAxis, int width, int height,
                    D3DXCOLOR color) {
  D3DXVECTOR3 siz;
  siz.x = width;
  siz.y = height;
  D3DXVECTOR3 pos;
  pos.x = xAxis;
  pos.y = yAxis;
  bool ck = false;

  D3DXVECTOR3 single = RotatePoint(EneamyPos, LocalPos, pos.x, pos.y, siz.x,
                                   siz.y, LocalPlayerY, 0.3f, &ck);
  if (eneamyDist >= 0.f && eneamyDist < RadarSettings::distance_Radar) {
    draw_team_point(single.x, single.y, team_id);
  }
}
// MiniMap Radar Stuff
void MiniMapRadar(D3DXVECTOR3 EneamyPos, D3DXVECTOR3 LocalPos,
                  float LocalPlayerY, float eneamyDist, int team_id,
                  float targetyaw) {
  ImGuiStyle *style = &ImGui::GetStyle();
  style->WindowRounding = 0.2f;
  ImGui::PushStyleColor(ImGuiCol_WindowBg,
                        ImVec4(0.13529413f, 0.14705884f, 0.15490198f, 0.82f));
  ImGuiWindowFlags TargetFlags;
  TargetFlags = ImGuiWindowFlags_::ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_::ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_::ImGuiWindowFlags_NoBackground |
                ImGuiWindowFlags_::ImGuiWindowFlags_NoTitleBar;
  // Remove the NoMove to move the minimap pos
  // you have to hit insert to bring up the hack menu, then while the menu is up
  // hit the windows kep to bring up the window start menu then just clikc back
  // on the middle of the screen to be on the overlay from there you can click
  // and drag the minmap around
  if (!firstS) {
    ImGui::SetNextWindowPos(ImVec2{1200, 60}, ImGuiCond_Once);
    firstS = true;
  }
  if (RadarSettings::Radar == true) {
    ImGui::SetNextWindowSize({250, 250});
    ImGui::Begin(("Radar"), 0, TargetFlags);
    {
      ImVec2 DrawPos = ImGui::GetCursorScreenPos();
      ImVec2 DrawSize = ImGui::GetContentRegionAvail();
      ImVec2 midRadar =
          ImVec2(DrawPos.x + (DrawSize.x / 2), DrawPos.y + (DrawSize.y / 2));
      if (global_settings.mini_map_guides) {
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(midRadar.x - DrawSize.x / 2.f, midRadar.y),
            ImVec2(midRadar.x + DrawSize.x / 2.f, midRadar.y),
            IM_COL32(255, 255, 255, 255));
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(midRadar.x, midRadar.y - DrawSize.y / 2.f),
            ImVec2(midRadar.x, midRadar.y + DrawSize.y / 2.f),
            IM_COL32(255, 255, 255, 255));
      }

      DrawRadarPointMiniMap(EneamyPos, LocalPos, LocalPlayerY, eneamyDist,
                            team_id, DrawPos.x, DrawPos.y, DrawSize.x,
                            DrawSize.y, {255, 255, 255, 255}, targetyaw);
    }
    ImGui::End();
  }
  ImGui::PopStyleColor();
}

// bool IsKeyDown(int vk)
// {
// 	return (GetAsyncKeyState(vk) & 0x8000) != 0;
// }
bool IsKeyDown(ImGuiKey imgui_k) { return ImGui::IsKeyPressed(imgui_k); }

// Full map radar test, Needs Manual setting of cords
// ImVec2 can be replaced with Vector2D
class world {
public:
  ImVec2 w1; // origin of point 1
  ImVec2 w2; // origin of point 2
  ImVec2 s1; // screen coord of point 1
  ImVec2 s2; // screen coord of point 2
  float ratioX;
  float ratioY;
  world(ImVec2 w1, ImVec2 s1, ImVec2 w2, ImVec2 s2) {
    this->w1 = w1;
    this->w2 = w2;
    this->s1 = s1;
    this->s2 = s2;
    this->ratioX = (s2.x - s1.x) / (w2.x - w1.x);
    this->ratioY = (s1.y - s2.y) / (w2.y - w1.y);
  }
};
// These values only work with 1920x1080 fullscreen, you have to redo the values
// for anything else..
//
//  Take screenshot, First is top right random pos, then bttm left random pos
//  from screen shot
//
//  First set is the x cord, then the y cord, then the screen x,y from the
//  screenshot, do the same for the second set. 1440p is x1.333333

world KingsCanyon(ImVec2(25223.177734, 28906.144531), ImVec2(1197, 185),
                  ImVec2(10399.223633, 13334.792969),
                  ImVec2(1014, 381)); // could be more accurate

world WorldsEdge(ImVec2(20501.476562, 33754.492188), ImVec2(1159, 127),
                 ImVec2(-4714.299805, -54425.144531),
                 ImVec2(622, 755)); // mp_rr_desertlands_hu - could be more
                                    // accurate  updated 7/16/2023

world Olympus(ImVec2(0, 0), ImVec2(0, 0), ImVec2(0, 0),
              ImVec2(0, 0)); // to be measured

world BrokenMoon(ImVec2(35159.300781, 30436.917969), ImVec2(1368, 151),
                 ImVec2(-30641.98821, -30347.98821),
                 ImVec2(593, 873)); // mp_rr_divided_moon - could be more
                                    // accurate  updated 7/16/2023

world StormPoint(ImVec2(34453.894531, 34695.917969), ImVec2(1264, 172),
                 ImVec2(-28786.898438, -16240.749023),
                 ImVec2(636,
                        677)); // mp_rr_tropic_island_mu1_storm updated - is
                               // within a few pixels of accuracy 7/16/2023

// DONE get map auto
void worldToScreenMap(D3DXVECTOR3 origin, int team_id) {
  float ratioX;
  float ratioY;
  ImVec2 w1;
  ImVec2 s1;
  // Is it me being lazy? or that i dont know how? prob both. True or False for
  // the map detection, set in the overlay menu.
  if (map == 1) { // Storm Point
    ratioX = StormPoint.ratioX;
    ratioY = StormPoint.ratioY;
    w1 = StormPoint.w1;
    s1 = StormPoint.s1;
  } else if (map == 2) { // KingsCanyon
    ratioX = KingsCanyon.ratioX;
    ratioY = KingsCanyon.ratioY;
    w1 = KingsCanyon.w1;
    s1 = KingsCanyon.s1;
  } else if (map == 3) { // WorldsEdge
    ratioX = WorldsEdge.ratioX;
    ratioY = WorldsEdge.ratioY;
    w1 = WorldsEdge.w1;
    s1 = WorldsEdge.s1;
  } else if (map == 4) { // Olympus
    ratioX = Olympus.ratioX;
    ratioY = Olympus.ratioY;
    w1 = Olympus.w1;
    s1 = Olympus.s1;
  } else if (map == 5) { // BrokenMoon
    ratioX = BrokenMoon.ratioX;
    ratioY = BrokenMoon.ratioY;
    w1 = BrokenMoon.w1;
    s1 = BrokenMoon.s1;
  } else {
    return;
  }

  // difference from location 1
  float world_diff_x = origin.x - w1.x;
  float world_diff_y = origin.y - w1.y;

  // get the screen offsets by applying the ratio
  float scr_diff_x = world_diff_x * ratioX;
  float scr_diff_y = world_diff_y * ratioY;

  // for x, add the offset to the screen x of location 1
  // for y, subtract the offset from the screen y of location 1 (cuz Y is from
  // bottom to up in Apex but it's from up to bottom in screen)
  float pos_x = s1.x + scr_diff_x;
  float pos_y = s1.y - scr_diff_y;

  draw_team_point(pos_x, pos_y, team_id);
}

void Overlay::RenderEsp() {
  next2 = false;
  if (g_Base != 0 && global_settings.esp) {

    memset(players, 0, sizeof(players));

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)getWidth(), (float)getHeight()));
    ImGui::Begin(XorStr("##esp"), (bool *)true,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoBackground |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);

    if (global_settings.show_aim_target && aim_target != Vector(0, 0, 0)) {
      Vector bs = Vector();
      WorldToScreen(aim_target, view_matrix_data.matrix, getWidth(),
                    getHeight(), bs);
      ImGui::GetWindowDrawList()->AddRectFilled(
          ImVec2(bs.x - 10, bs.y - 10), ImVec2(bs.x + 10, bs.y + 10),
          ImColor(1.0f, 0.647f, 0.0f, 0.6f), 10.0f, 0);
    }

    if (!global_settings.firing_range)
      while (!next2 && global_settings.esp) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
      }

    if (next2 && valid) {

      for (int i = 0; i < 100; i++) {

        if (players[i].health > 0) {
          std::string distance = std::to_string(players[i].dist / 39.62);
          distance = distance.substr(0, distance.find('.')) + "m(" +
                     std::to_string(players[i].entity_team) + ")";

          float alpha; // The farther away, the more transparent
          if (players[i].dist < global_settings.aim_dist) {
            alpha = 1.0f;
          } else if (players[i].dist > 16000.0f) {
            alpha = 0.4f;
          } else {
            alpha = 1.0f -
                    ((players[i].dist - global_settings.aim_dist) / (16000.0f - global_settings.aim_dist) * 0.6f);
          }

          float radardistance = (int)(players[i].dist / 39.62);

          // Radar Stuff
          if (global_settings.mini_map_radar == true) {
            MiniMapRadar(players[i].EntityPosition,
                         players[i].LocalPlayerPosition,
                         players[i].localviewangle.y, radardistance,
                         players[i].entity_team, players[i].targetyaw);
          }
          if (v.line)
            DrawLine(ImVec2((float)(getWidth() / 2.0), (float)getHeight()),
                     ImVec2(players[i].b_x, players[i].b_y), BLUE,
                     1); // LINE FROM MIDDLE SCREEN

          if (v.distance) {
            if (players[i].knocked)
              String(ImVec2(players[i].boxMiddle, (players[i].b_y + 1)), RED,
                     distance.c_str()); // DISTANCE
            else
              String(ImVec2(players[i].boxMiddle, (players[i].b_y + 1)),
                     ImColor(0.0f, 1.0f, 0.0f, alpha),
                     distance.c_str()); // DISTANCE
          }

          if (players[i].dist < 16000.0f) {
            if (v.healthbar)
              DrawSeerLikeHealth(
                  (players[i].b_x - (players[i].width / 2.0f) + 5),
                  (players[i].b_y - players[i].height - 10), players[i].shield,
                  players[i].maxshield, players[i].armortype,
                  players[i].health); // health bar

            if (v.box) {
              ImColor box_color = ImColor(0.0f, 0.0f, 0.0f, alpha);
              float box_width = 1.0f;
              if (players[i].visible) {
                box_color = ImColor(global_settings.glow_r_viz,
                                    global_settings.glow_g_viz,
                                    global_settings.glow_b_viz, alpha);
                box_width = 2.0f;
              } else {
                box_color = ImColor(global_settings.glow_r_not,
                                    global_settings.glow_g_not,
                                    global_settings.glow_b_not, alpha);
              }
              DrawBox(box_color, players[i].b_x - (players[i].width / 2.0f),
                      players[i].b_y - players[i].height, players[i].width,
                      players[i].height, box_width);
            }
            if (v.name)
              String(ImVec2(players[i].boxMiddle,
                            (players[i].b_y - players[i].height - 15)),
                     ImColor(1.0f, 1.0f, 1.0f, alpha), players[i].name);
          }
          // Full Radar map, Need Manual setting of cords
          if (global_settings.main_radar_map == true)

            worldToScreenMap(players[i].EntityPosition, players[i].entity_team);

          // String(ImVec2(players[i].boxMiddle, (players[i].b_y -
          // players[i].height - 15)), WHITE, players[i].name);
        }
      }
    }
    ImGui::End();
  }
}

void start_overlay() {
  overlay_t = true;

  Overlay ov1 = Overlay();
  printf(XorStr("Waiting for The Extra Ban .... Never Gonna Get it!\n"));

  std::thread ui_thr = ov1.Start();
  ui_thr.detach();

  // while (check == 0xABCD) {
  //   printf(XorStr("Never Gonna Get it!\n"));
  //   std::this_thread::sleep_for(std::chrono::seconds(1));
  // }
  ready = true;

  while (overlay_t) {
    // if (IsKeyDown(ImGuiKey_F4)) {
    //   active = false;
    //   break;
    // }
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  ready = false;
  ov1.Clear();
}

// int main(int argc, char **argv) {
//   start_overlay();
//   return 0;
// }