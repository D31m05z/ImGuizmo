#include <imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "ImGuizmo.h"
#include "ImSequencer.h"
#include "ImCurveEdit.h"
#include "imgui_internal.h"

#include <math.h>
#include <vector>
#include <algorithm>
#include <stdexcept>

//
//
// ImGuizmo example helper functions
//
//
static inline ImVec2 operator-(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y); }

void Frustum(float left, float right, float bottom, float top, float znear, float zfar, float *m16)
{
  float temp, temp2, temp3, temp4;
  temp = 2.0f * znear;
  temp2 = right - left;
  temp3 = top - bottom;
  temp4 = zfar - znear;
  m16[0] = temp / temp2;
  m16[1] = 0.0;
  m16[2] = 0.0;
  m16[3] = 0.0;
  m16[4] = 0.0;
  m16[5] = temp / temp3;
  m16[6] = 0.0;
  m16[7] = 0.0;
  m16[8] = (right + left) / temp2;
  m16[9] = (top + bottom) / temp3;
  m16[10] = (-zfar - znear) / temp4;
  m16[11] = -1.0f;
  m16[12] = 0.0;
  m16[13] = 0.0;
  m16[14] = (-temp * zfar) / temp4;
  m16[15] = 0.0;
}

void Perspective(float fovyInDegrees, float aspectRatio, float znear, float zfar, float *m16)
{
  float ymax, xmax;
  ymax = znear * tanf(fovyInDegrees * 3.141592f / 180.0f);
  xmax = ymax * aspectRatio;
  Frustum(-xmax, xmax, -ymax, ymax, znear, zfar, m16);
}

void Cross(const float* a, const float* b, float* r)
{
  r[0] = a[1] * b[2] - a[2] * b[1];
  r[1] = a[2] * b[0] - a[0] * b[2];
  r[2] = a[0] * b[1] - a[1] * b[0];
}

float Dot(const float* a, const float* b)
{
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void Normalize(const float* a, float *r)
{
  float il = 1.f / (sqrtf(Dot(a, a)) + FLT_EPSILON);
  r[0] = a[0] * il;
  r[1] = a[1] * il;
  r[2] = a[2] * il;
}

void LookAt(const float* eye, const float* at, const float* up, float *m16)
{
  float X[3], Y[3], Z[3], tmp[3];

  tmp[0] = eye[0] - at[0];
  tmp[1] = eye[1] - at[1];
  tmp[2] = eye[2] - at[2];
  //Z.normalize(eye - at);
  Normalize(tmp, Z);
  Normalize(up, Y);
  //Y.normalize(up);

  Cross(Y, Z, tmp);
  //tmp.cross(Y, Z);
  Normalize(tmp, X);
  //X.normalize(tmp);

  Cross(Z, X, tmp);
  //tmp.cross(Z, X);
  Normalize(tmp, Y);
  //Y.normalize(tmp);

  m16[0] = X[0];
  m16[1] = Y[0];
  m16[2] = Z[0];
  m16[3] = 0.0f;
  m16[4] = X[1];
  m16[5] = Y[1];
  m16[6] = Z[1];
  m16[7] = 0.0f;
  m16[8] = X[2];
  m16[9] = Y[2];
  m16[10] = Z[2];
  m16[11] = 0.0f;
  m16[12] = -Dot(X, eye);
  m16[13] = -Dot(Y, eye);
  m16[14] = -Dot(Z, eye);
  m16[15] = 1.0f;
}

void OrthoGraphic(const float l, float r, float b, const float t, float zn, const float zf, float *m16)
{
  m16[0] = 2 / (r - l);
  m16[1] = 0.0f;
  m16[2] = 0.0f;
  m16[3] = 0.0f;
  m16[4] = 0.0f;
  m16[5] = 2 / (t - b);
  m16[6] = 0.0f;
  m16[7] = 0.0f;
  m16[8] = 0.0f;
  m16[9] = 0.0f;
  m16[10] = 1.0f / (zf - zn);
  m16[11] = 0.0f;
  m16[12] = (l + r) / (l - r);
  m16[13] = (t + b) / (b - t);
  m16[14] = zn / (zn - zf);
  m16[15] = 1.0f;
}

void EditTransform(const float *cameraView, float *cameraProjection, float* matrix)
{
  static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
  static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::LOCAL);
  static bool useSnap = false;
  static float snap[3] = { 1.f, 1.f, 1.f };
  static float bounds[] = { -0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f };
  static float boundsSnap[] = { 0.1f, 0.1f, 0.1f };
  static bool boundSizing = false;
  static bool boundSizingSnap = false;

  if (ImGui::IsKeyPressed(90)) {
    mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
  }
  if (ImGui::IsKeyPressed(69)) {
    mCurrentGizmoOperation = ImGuizmo::ROTATE;
  }
  if (ImGui::IsKeyPressed(82)) { // r Key
    mCurrentGizmoOperation = ImGuizmo::SCALE;
  }
  if (ImGui::RadioButton("Translate", mCurrentGizmoOperation == ImGuizmo::TRANSLATE)) {
    mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
  }

  ImGui::SameLine();

  if (ImGui::RadioButton("Rotate", mCurrentGizmoOperation == ImGuizmo::ROTATE)) {
    mCurrentGizmoOperation = ImGuizmo::ROTATE;
  }

  ImGui::SameLine();

  if (ImGui::RadioButton("Scale", mCurrentGizmoOperation == ImGuizmo::SCALE)) {
    mCurrentGizmoOperation = ImGuizmo::SCALE;
  }

  float matrixTranslation[3], matrixRotation[3], matrixScale[3];

  ImGuizmo::DecomposeMatrixToComponents(matrix, matrixTranslation, matrixRotation, matrixScale);
  ImGui::InputFloat3("Tr", matrixTranslation, 3);
  ImGui::InputFloat3("Rt", matrixRotation, 3);
  ImGui::InputFloat3("Sc", matrixScale, 3);
  ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, matrix);

  if (mCurrentGizmoOperation != ImGuizmo::SCALE) {
    if (ImGui::RadioButton("Local", mCurrentGizmoMode == ImGuizmo::LOCAL)) {
      mCurrentGizmoMode = ImGuizmo::LOCAL;
    }

    ImGui::SameLine();

    if (ImGui::RadioButton("World", mCurrentGizmoMode == ImGuizmo::WORLD)) {
      mCurrentGizmoMode = ImGuizmo::WORLD;
    }
  }

  if (ImGui::IsKeyPressed(83)) {
    useSnap = !useSnap;
  }

  ImGui::Checkbox("", &useSnap);
  ImGui::SameLine();

  switch (mCurrentGizmoOperation) {
  case ImGuizmo::TRANSLATE:
    ImGui::InputFloat3("Snap", &snap[0]);
    break;
  case ImGuizmo::ROTATE:
    ImGui::InputFloat("Angle Snap", &snap[0]);
    break;
  case ImGuizmo::SCALE:
    ImGui::InputFloat("Scale Snap", &snap[0]);
    break;
  }

  ImGui::Checkbox("Bound Sizing", &boundSizing);
  if (boundSizing) {
    ImGui::PushID(3);
    ImGui::Checkbox("", &boundSizingSnap);
    ImGui::SameLine();
    ImGui::InputFloat3("Snap", boundsSnap);
    ImGui::PopID();
  }

  ImGuiIO& io = ImGui::GetIO();
  ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
  ImGuizmo::Manipulate(cameraView, cameraProjection, mCurrentGizmoOperation, mCurrentGizmoMode, matrix, NULL, useSnap ? &snap[0] : NULL, boundSizing?bounds:NULL, boundSizingSnap?boundsSnap:NULL);
}

//
//
// ImSequencer interface
//
//
static const char* SequencerItemTypeNames[] = { "Camera","Music", "ScreenEffect", "FadeIn", "Animation" };


struct RampEdit : public ImCurveEdit::Delegate
{
   RampEdit()
   {
      mPts[0][0] = ImVec2(-10.f, 0);
      mPts[0][1] = ImVec2(20.f, 0.6f);
      mPts[0][2] = ImVec2(25.f, 0.2f);
      mPts[0][3] = ImVec2(70.f, 0.4f);
      mPts[0][4] = ImVec2(120.f, 1.f);
      mPointCount[0] = 5;

      mPts[1][0] = ImVec2(-50.f, 0.2f);
      mPts[1][1] = ImVec2(33.f, 0.7f);
      mPts[1][2] = ImVec2(80.f, 0.2f);
      mPts[1][3] = ImVec2(82.f, 0.8f);
      mPointCount[1] = 4;


      mPts[2][0] = ImVec2(40.f, 0);
      mPts[2][1] = ImVec2(60.f, 0.1f);
      mPts[2][2] = ImVec2(90.f, 0.82f);
      mPts[2][3] = ImVec2(150.f, 0.24f);
      mPts[2][4] = ImVec2(200.f, 0.34f);
      mPts[2][5] = ImVec2(250.f, 0.12f);
      mPointCount[2] = 6;
      mbVisible[0] = mbVisible[1] = mbVisible[2] = true;
      mMax = ImVec2(1.f, 1.f);
      mMin = ImVec2(0.f, 0.f);
   }

   size_t GetCurveCount()
   {
      return 3;
   }

   bool IsVisible(size_t curveIndex)
   {
      return mbVisible[curveIndex];
   }

   size_t GetPointCount(size_t curveIndex)
   {
      return mPointCount[curveIndex];
   }

   uint32_t GetCurveColor(size_t curveIndex)
   {
      uint32_t cols[] = { 0xFF0000FF, 0xFF00FF00, 0xFFFF0000 };
      return cols[curveIndex];
   }

   ImVec2* GetPoints(size_t curveIndex)
   {
      return mPts[curveIndex];
   }

   virtual ImCurveEdit::CurveType GetCurveType(size_t curveIndex) const { return ImCurveEdit::CurveSmooth; }

   virtual int EditPoint(size_t curveIndex, int pointIndex, ImVec2 value)
   {
      mPts[curveIndex][pointIndex] = ImVec2(value.x, value.y);
      SortValues(curveIndex);

      for (size_t i = 0; i < GetPointCount(curveIndex); i++) {
         if (mPts[curveIndex][i].x == value.x)
            return i;
      }
      return pointIndex;
   }

   virtual void AddPoint(size_t curveIndex, ImVec2 value)
   {
      if (mPointCount[curveIndex] >= 8)
         return;

      mPts[curveIndex][mPointCount[curveIndex]++] = value;
      SortValues(curveIndex);
   }

   virtual ImVec2& GetMax() { return mMax; }
   virtual ImVec2& GetMin() { return mMin; }

   virtual unsigned int GetBackgroundColor() { return 0; }
   ImVec2 mPts[3][8];
   size_t mPointCount[3];
   bool mbVisible[3];
   ImVec2 mMin;
   ImVec2 mMax;

private:
   void SortValues(size_t curveIndex)
   {
      auto b = std::begin(mPts[curveIndex]);
      auto e = std::begin(mPts[curveIndex]) + GetPointCount(curveIndex);
      std::sort(b, e, [](ImVec2 a, ImVec2 b) { return a.x < b.x; });
   }
};

struct MySequence : public ImSequencer::SequenceInterface
{
  // interface with sequencer

   virtual int GetFrameMin() const {
      return mFrameMin;
   }

   virtual int GetFrameMax() const {
      return mFrameMax;
   }

  virtual int GetItemCount() const { return (int)myItems.size(); }

  virtual int GetItemTypeCount() const { return sizeof(SequencerItemTypeNames)/sizeof(char*); }

  virtual const char *GetItemTypeName(int typeIndex) const { return SequencerItemTypeNames[typeIndex]; }

  virtual const char *GetItemLabel(int index) const
  {
    static char tmps[512];
    sprintf(tmps, "[%02d] %s", index, SequencerItemTypeNames[myItems[index].mType]);
    return tmps;
  }

  virtual void Get(int index, int** start, int** end, int *type, unsigned int *color)
  {
    MySequenceItem &item = myItems[index];
    if (color)
      *color = 0xFFAA8080; // same color for everyone, return color based on type
    if (start)
      *start = &item.mFrameStart;
    if (end)
      *end = &item.mFrameEnd;
    if (type)
      *type = item.mType;
  }

  virtual void Add(int type) { myItems.push_back(MySequenceItem{ type, 0, 10, false }); };

  virtual void Del(int index) { myItems.erase(myItems.begin() + index); }

  virtual void Duplicate(int index) { myItems.push_back(myItems[index]); }

  virtual size_t GetCustomHeight(int index) { return myItems[index].mExpanded ? 300 : 0; }

   // my datas
  MySequence() : mFrameMin(0), mFrameMax(0) {}
  int mFrameMin, mFrameMax;
  struct MySequenceItem
  {
    int mType;
    int mFrameStart, mFrameEnd;
    bool mExpanded;
  };

  std::vector<MySequenceItem> myItems;
  RampEdit rampEdit;

  virtual void DoubleClick(int index) {
      if (myItems[index].mExpanded) {
         myItems[index].mExpanded = false;
         return;
      }

      for (auto& item : myItems) {
         item.mExpanded = false;
       }

      myItems[index].mExpanded = !myItems[index].mExpanded;
   }

   virtual void CustomDraw(int index, ImDrawList* draw_list, const ImRect& rc, const ImRect& legendRect, const ImRect& clippingRect, const ImRect& legendClippingRect)
   {
      static const char *labels[] = { "Translation", "Rotation" , "Scale" };

      rampEdit.mMax = ImVec2(float(mFrameMax), 1.f);
      rampEdit.mMin = ImVec2(float(mFrameMin), 0.f);
      draw_list->PushClipRect(legendClippingRect.Min, legendClippingRect.Max, true);

      for (int i = 0; i < 3; i++) {
         ImVec2 pta(legendRect.Min.x + 30, legendRect.Min.y + i * 14.f);
         ImVec2 ptb(legendRect.Max.x, legendRect.Min.y + (i+1) * 14.f);
         draw_list->AddText(pta, rampEdit.mbVisible[i]?0xFFFFFFFF:0x80FFFFFF, labels[i]);
         if (ImRect(pta, ptb).Contains(ImGui::GetMousePos()) && ImGui::IsMouseClicked(0)) {
            rampEdit.mbVisible[i] = !rampEdit.mbVisible[i];
          }
      }

      draw_list->PopClipRect();

      ImGui::SetCursorScreenPos(rc.Min);
      ImCurveEdit::Edit(rampEdit, rc.Max-rc.Min, 137 + index, &clippingRect);
   }

   virtual void CustomDrawCompact(int index, ImDrawList* draw_list, const ImRect& rc, const ImRect& clippingRect)
   {
      rampEdit.mMax = ImVec2(float(mFrameMax), 1.f);
      rampEdit.mMin = ImVec2(float(mFrameMin), 0.f);

      draw_list->PushClipRect(clippingRect.Min, clippingRect.Max, true);

      for (int i = 0; i < 3; i++) {
         for (int j = 0; j < rampEdit.mPointCount[i]; j++) {
            float p = rampEdit.mPts[i][j].x;
            if (p < myItems[index].mFrameStart || p > myItems[index].mFrameEnd) {
               continue;
            }

            float r = (p - mFrameMin) / float(mFrameMax - mFrameMin);
            float x = ImLerp(rc.Min.x, rc.Max.x, r);
            draw_list->AddLine(ImVec2(x, rc.Min.y + 6), ImVec2(x, rc.Max.y - 4), 0xAA000000, 4.f);
         }
      }
      draw_list->PopClipRect();
   }
};

inline void rotationY(const float angle, float *m16)
{
  float c = cosf(angle);
  float s = sinf(angle);

  m16[0] = c;
  m16[1] = 0.0f;
  m16[2] = -s;
  m16[3] = 0.0f;
  m16[4] = 0.0f;
  m16[5] = 1.f;
  m16[6] = 0.0f;
  m16[7] = 0.0f;
  m16[8] = s;
  m16[9] = 0.0f;
  m16[10] = c;
  m16[11] = 0.0f;
  m16[12] = 0.f;
  m16[13] = 0.f;
  m16[14] = 0.f;
  m16[15] = 1.0f;
}

static void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error %d: %s\n", error, description);
}

int main(int, char**)
{
    GLFWwindow* window = nullptr;

    // Setup window
    glfwSetErrorCallback(error_callback);
    if (!glfwInit()) {
        throw std::runtime_error("[E] glfwInit failed");
    }

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (monitor == nullptr) {
        throw std::runtime_error("Could not get primary monitor. This is probably caused by using RDP.");
    }

    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    if (mode == nullptr) {
        throw std::runtime_error("Could not get video mode. This is probably caused by using RDP.");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#ifdef GLFW_CONTEXT_CREATION_API
    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
#endif
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
    glfwWindowHint(GLFW_RED_BITS, mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
    glfwWindowHint(GLFW_ALPHA_BITS, 0);
    glfwWindowHint(GLFW_DECORATED, true);

    window = glfwCreateWindow(1280, 720, "imguizmo example", nullptr, nullptr);

    if (window == nullptr) {
        throw std::runtime_error("Could not create GLFW window.");
    }
    glfwMakeContextCurrent(window);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 100");

    float objectMatrix[16] =
      { 1.f, 0.f, 0.f, 0.f,
      0.f, 1.f, 0.f, 0.f,
      0.f, 0.f, 1.f, 0.f,
      0.f, 0.f, 0.f, 1.f };

    static const float identityMatrix[16] =
    { 1.f, 0.f, 0.f, 0.f,
      0.f, 1.f, 0.f, 0.f,
      0.f, 0.f, 1.f, 0.f,
      0.f, 0.f, 0.f, 1.f };

    float cameraView[16] =
      { 1.f, 0.f, 0.f, 0.f,
      0.f, 1.f, 0.f, 0.f,
      0.f, 0.f, 1.f, 0.f,
      0.f, 0.f, 0.f, 1.f };

    float cameraProjection[16];

    // sequence with default values
    MySequence mySequence;
    mySequence.mFrameMin = -100;
    mySequence.mFrameMax = 1000;
    mySequence.myItems.push_back(MySequence::MySequenceItem{ 0, 10, 30, false });
    mySequence.myItems.push_back(MySequence::MySequenceItem{ 1, 20, 30, true });
    mySequence.myItems.push_back(MySequence::MySequenceItem{ 3, 12, 60, false });
    mySequence.myItems.push_back(MySequence::MySequenceItem{ 2, 61, 90, false });
    mySequence.myItems.push_back(MySequence::MySequenceItem{ 4, 90, 99, false });

    // Camera projection
    bool isPerspective = false;
    float fov = 27.f;
    float viewWidth = 10.f; // for orthographic
    float camYAngle = 165.f / 180.f * 3.14159f;
    float camXAngle = 52.f / 180.f * 3.14159f;
    float camDistance = 8.f;
    rotationY(0.f, objectMatrix);

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();

        // start imgui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiIO& io = ImGui::GetIO();
        if (isPerspective) {
          Perspective(fov, io.DisplaySize.x / io.DisplaySize.y, 0.1f, 100.f, cameraProjection);
        } else {
          float viewHeight = viewWidth*io.DisplaySize.y / io.DisplaySize.x;
          OrthoGraphic(-viewWidth, viewWidth, -viewHeight, viewHeight, -viewWidth, viewWidth, cameraProjection);
        }

        ImGuizmo::SetOrthographic(!isPerspective);

        float eye[] = { cosf(camYAngle) * cosf(camXAngle) * camDistance, sinf(camXAngle) * camDistance, sinf(camYAngle) * cosf(camXAngle) * camDistance };
        float at[] = { 0.f, 0.f, 0.f };
        float up[] = { 0.f, 1.f, 0.f };

        LookAt(eye, at, up, cameraView);
        ImGuizmo::BeginFrame();

        //static float ng = 0.f;
        //ng += 0.01f;
        //ng = 1.f;
        //rotationY(ng, objectMatrix);
        //objectMatrix[12] = 5.f;

        // debug
        ImGuizmo::DrawCube(cameraView, cameraProjection, objectMatrix);
        ImGuizmo::DrawGrid(cameraView, cameraProjection, identityMatrix, 10.f);

        // create a window and insert the inspector
        //ImGui::SetNextWindowPos(ImVec2(10, 10));
        //ImGui::SetNextWindowSize(ImVec2(320, 340));

        ImGui::Begin("Editor");
        ImGui::Text("Camera");

        if (ImGui::RadioButton("Perspective", isPerspective))
            isPerspective = true;

        ImGui::SameLine();

        if (ImGui::RadioButton("Orthographic", !isPerspective))
            isPerspective = false;

        if (isPerspective) {
          ImGui::SliderFloat("Fov", &fov, 20.f, 110.f);
        } else {
          ImGui::SliderFloat("Ortho width", &viewWidth, 1, 20);
        }

        ImGui::SliderAngle("Camera X", &camXAngle, 0.f, 179.f);
        ImGui::SliderAngle("Camera Y", &camYAngle);
        ImGui::SliderFloat("Distance", &camDistance, 1.f, 10.f);
        ImGui::Text("X: %f Y: %f", io.MousePos.x, io.MousePos.y);

        ImGui::Separator();

        EditTransform(cameraView, cameraProjection, objectMatrix);
        ImGui::End();   //  end Editor

        // let's create the sequencer
        static int selectedEntry = -1;
        static int firstFrame = 0;
        static bool expanded = true;
        static int currentFrame = 100;

        //ImGui::SetNextWindowPos(ImVec2(10, 350));
        //ImGui::SetNextWindowSize(ImVec2(940, 480));

        ImGui::Begin("Sequencer");
        ImGui::PushItemWidth(130);
        ImGui::InputInt("Frame Min", &mySequence.mFrameMin);
        ImGui::SameLine();
        ImGui::InputInt("Frame ", &currentFrame);
        ImGui::SameLine();
        ImGui::InputInt("Frame Max", &mySequence.mFrameMax);
        ImGui::PopItemWidth();

        Sequencer(&mySequence, &currentFrame, &expanded, &selectedEntry, &firstFrame, ImSequencer::SEQUENCER_EDIT_STARTEND | ImSequencer::SEQUENCER_ADD | ImSequencer::SEQUENCER_DEL | ImSequencer::SEQUENCER_COPYPASTE | ImSequencer::SEQUENCER_CHANGE_FRAME);
        // add a UI to edit that particular item
        if (selectedEntry != -1) {
          const MySequence::MySequenceItem &item = mySequence.myItems[selectedEntry];
          ImGui::Text("I am a %s, please edit me", SequencerItemTypeNames[item.mType]);
          // switch (type) ....
        }
        ImGui::End();   //  end Sequencer

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwMakeContextCurrent(window);
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.4f, 0.4f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwMakeContextCurrent(window);
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
