#include "input.h"

#include "model/model.h"
#include "renderer.h"
#include "context.h"

#include <algorithm>

class Context;

void InputHandler::handleKeystate()
{
  /* use GetKeyState to insert pressed keys into _keyState or remove them if they're not pressed anymore */
  std::unordered_set<int> newState;
  int c;
  while ((c = GetKeyPressed()))
    newState.insert(c);

  /* trigger events */
  for (int key : _keyState)
  {
    if (newState.find(key) == newState.end())
      keyUp(key);
  }

  for (int key : newState)
  {
    if (_keyState.find(key) == _keyState.end())
      keyDown(key);
  }

  _keyState = newState;
}


void InputHandler::handle(nb::Model* model)
{
  this->model = model;

  handleKeystate();

  vec2 position = GetMousePosition();

  bool any = false;
  for (layer_index_t i = 0; i < model->layerCount(); ++i)
  {
    float y = (model->size().height * Data::Constants::LAYER2D_CELL_SIZE.height) * i + (Data::Constants::LAYER2D_SPACING * i);
    rect bounds = rect(_context->prefs.gridTopPosition().x, _context->prefs.gridTopPosition().y + y, model->size().width * Data::Constants::LAYER2D_CELL_SIZE.width, model->size().height * Data::Constants::LAYER2D_CELL_SIZE.height);

    /* if mouse is inside 2d layer grid */
    if (bounds.CheckCollision(position))
    {
      auto relative = position - bounds.Origin();

      /* if snap mode is free we should adjust position to center it around piece according to piece size */
      if (_context->prefs.ui.grid.halfSteps == GridSnapMode::Free || _context->prefs.ui.grid.centerPieceInHover)
      {
        relative.x -= (Data::Constants::LAYER2D_CELL_SIZE.width * (_context->brush->width())) / 2;
        relative.y -= (Data::Constants::LAYER2D_CELL_SIZE.height * (_context->brush->height())) / 2;
      }

      /* this value is in half steps */
      vec2 cell = vec2(relative.x / Data::Constants::LAYER2D_CELL_SIZE.width, relative.y / Data::Constants::LAYER2D_CELL_SIZE.height);

      /* we need to convert half steps grid into a nb::PieceCoord according to the snap mode */
      if (_context->prefs.ui.grid.halfSteps == GridSnapMode::Whole)
      {
        /* we drop fractional part and align to whole block */
        cell.x = std::floor(cell.x);
        cell.y = std::floor(cell.y);
      }
      else if (_context->prefs.ui.grid.halfSteps == GridSnapMode::Half)
      {
        /* all values < 0.5 are floored, otherwise ceiled */
        cell.x = (cell.x - std::floor(cell.x) < 0.5f) ? std::floor(cell.x) : (std::floor(cell.x) + 0.5f);
        cell.y = (cell.y - std::floor(cell.y) < 0.5f) ? std::floor(cell.y) : (std::floor(cell.y) + 0.5f);
      }
      else
        ;

      _hover = nb::PieceCoord3d(_context->renderer->_topDown.begin().index() - i, nb::PieceCoord(cell.x, cell.y));

      any = true;
      break;
    }
  }

  if (position != _mousePosition)
  {
    /* dispatch drag event changed if mouse button is down */
    for (size_t i = 0; i < _mouseState.size(); ++i)
    {
      if (_mouseState[i])
      {
        DragStatus nextStatus = (_dragStatus == DragStatus::None) ? DragStatus::Start : DragStatus::Change;
        mouseDrag(position, nextStatus, static_cast<MouseButton>(i));
        _dragStatus = nextStatus;
      }
    }

    _mousePosition = position;
  }

  if (!any)
    _hover.reset();

  /* fetch button state into a new std::array and call relevant methods if state changed */
  std::array<bool, 3> newState = { IsMouseButtonDown(MOUSE_LEFT_BUTTON), IsMouseButtonDown(MOUSE_MIDDLE_BUTTON), IsMouseButtonDown(MOUSE_RIGHT_BUTTON) };
  for (size_t i = 0; i < _mouseState.size(); ++i)
  {
    if (newState[i] != _mouseState[i])
    {
      if (newState[i])
        mouseDown(static_cast<MouseButton>(i));
      else
      {
        mouseUp(static_cast<MouseButton>(i));
        if (_dragStatus != DragStatus::None)
        {
          mouseDrag(position, DragStatus::End, static_cast<MouseButton>(i));
          _dragStatus = DragStatus::None;
        }
      }
    }
  }
  _mouseState = newState;

  handleMouseWheel(position);
}

bool InputHandler::isMouseOverTopDownGrid(const vec2& position) const
{
  const float layerWidth = model->size().width * Data::Constants::LAYER2D_CELL_SIZE.width;
  const float layerHeight = model->size().height * Data::Constants::LAYER2D_CELL_SIZE.height;
  const float shownLayers = static_cast<float>(std::min(_context->renderer->_topDown._shown, model->layerCount()));
  const float totalHeight = shownLayers * layerHeight + std::max(0.0f, shownLayers - 1.0f) * Data::Constants::LAYER2D_SPACING;
  const vec2 origin = _context->prefs.gridTopPosition();
  const rect bounds = rect(origin.x, origin.y, layerWidth, totalHeight);

  return bounds.CheckCollision(position);
}

void InputHandler::handleMouseWheel(const vec2& position)
{
  float v = GetMouseWheelMove();
  if (!v)
    return;

  if (isMouseOverTopDownGrid(position))
  {
    if (v < 0 && _context->renderer->_topDown._offset > 0)
      --_context->renderer->_topDown._offset;
    else if (v > 0 && _context->renderer->_topDown._offset + _context->renderer->_topDown._shown < model->layerCount())
      ++_context->renderer->_topDown._offset;

    return;
  }

  zoomCamera(v);
}

void InputHandler::panCamera(const vec2& delta)
{
  auto& cam = _context->renderer->camera();
  Vector3 offset = Vector3Subtract(cam.position, cam.target);
  float radius = Vector3Length(offset);

  Vector3 forward = Vector3Subtract(cam.target, cam.position);
  forward.y = 0.0f;

  if (Vector3Length(forward) <= 1e-5f)
    forward = { 0.0f, 0.0f, 1.0f };
  else
    forward = Vector3Normalize(forward);

  const Vector3 worldUp = { 0.0f, 1.0f, 0.0f };
  Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, worldUp));
  Vector3 movement = Vector3Add(Vector3Scale(right, -delta.x), Vector3Scale(forward, delta.y));

  if (Vector3Length(movement) <= 1e-5f)
    return;

  const float panScale = std::max(radius, Data::Constants::side * 3.0f) * 0.0015f;
  movement = Vector3Scale(movement, panScale);

  cam.position = Vector3Add(cam.position, movement);
  cam.target = Vector3Add(cam.target, movement);
}

void InputHandler::zoomCamera(float wheelMove)
{
  auto& cam = _context->renderer->camera();
  Vector3 offset = Vector3Subtract(cam.position, cam.target);
  float radius = Vector3Length(offset);

  if (radius <= 1e-5f)
    return;

  const float modelWidth = model->size().width * Data::Constants::side;
  const float modelDepth = model->size().height * Data::Constants::side;
  const float modelHeight = std::max(1, model->layerCount()) * Data::Constants::height;
  const float modelExtent = std::max({ modelWidth, modelDepth, modelHeight, Data::Constants::side });
  const float minRadius = modelExtent * 0.35f;
  const float maxRadius = modelExtent * 12.0f;
  const float zoomStep = 0.12f;

  float nextRadius = radius * (1.0f - wheelMove * zoomStep);
  nextRadius = std::clamp(nextRadius, minRadius, maxRadius);

  cam.position = Vector3Add(cam.target, Vector3Scale(Vector3Normalize(offset), nextRadius));
}

void InputHandler::mouseDown(MouseButton button)
{
  if (button == MouseButton::Left && _hover)
  {
    /* remove piece at hover position if present, otherwise add piece */
    nb::Piece* p = model->piece(*_hover);
    if (p)
      model->remove(p);
    else
    {
      nb::Piece piece = *_context->brush.get();
      piece.moveAt(_hover->coord);
      model->addPiece(_hover->z, piece);
    }
  }

}

void InputHandler::mouseUp(MouseButton button)
{
  if (button == MouseButton::Right && _dragStatus == DragStatus::None)
    _context->brush->swapSize();
}

void InputHandler::mouseDrag(const vec2& position, DragStatus status, MouseButton button)
{
  /* log on console drag event, use a switch to print status */
  if (status == DragStatus::Start)
    printf("Drag started at (%.2f, %.2f) with button %d\n", position.x, position.y, static_cast<int>(button));
  else if (status == DragStatus::Change)
  {
    Vector2 d = GetMouseDelta();

    if (button == MouseButton::Right)
    {
      panCamera(d);
      return;
    }

    if (button != MouseButton::Left)
      return;

    const float sens = 0.005f;   // sensibilità mouse (radiani per pixel)
    const float maxPitchDeg = 89.0f;
    const float maxVerticalAlignment = sinf(DEG2RAD * maxPitchDeg);


    auto& cam = _context->renderer->camera();

    Vector3 worldUp = { 0,1,0 };
    Vector3 off = Vector3Subtract(cam.position, cam.target);
    const float radius = Vector3Length(off);

    // Direzione attuale camera -> target (verso il modello)
    Vector3 dir = Vector3Normalize(Vector3Negate(off)); // from cam to target
    // Asse destro locale (right)
    Vector3 right = Vector3Normalize(Vector3CrossProduct(dir, worldUp));
    // Se dir ~ parallelo a worldUp (ai poli), stabilizza right
    if (Vector3Length(right) < 1e-5f) right = Vector3( 1,0,0 );

    // Yaw: attorno a worldUp
    Quaternion qYaw = QuaternionFromAxisAngle(worldUp, -d.x * sens);
    // Pitch: attorno a right (asse locale)
    Quaternion qPit = QuaternionFromAxisAngle(right, -d.y * sens);

    // Applica yaw e poi pitch all’offset
    Quaternion q = QuaternionMultiply(qPit, qYaw);
    Vector3 offNew = Vector3RotateByQuaternion(off, q);

    // Clamp del pitch: limito l’inclinazione rispetto alla verticale
    Vector3 dirNew = Vector3Normalize(Vector3Negate(offNew));
    float c = fabsf(Vector3DotProduct(dirNew, worldUp)); // 0 = orizzontale, 1 = verticale
    if (c > maxVerticalAlignment) {
      // Se supera il limite, accetta solo yaw (niente pitch)
      offNew = Vector3RotateByQuaternion(off, qYaw);
    }

    off = Vector3Scale(Vector3Normalize(offNew), radius);

    cam.position = Vector3Add(cam.target, off);
    
    //printf("Drag changed at (%.2f, %.2f) with button %d\n", position.x, position.y, static_cast<int>(button));
  }
  else if (status == DragStatus::End)
    printf("Drag ended at (%.2f, %.2f) with button %d\n", position.x, position.y, static_cast<int>(button));
}

void InputHandler::keyUp(int key)
{

}


void InputHandler::keyDown(int key)
{
  if (key == KEY_W)
  {
    _context->brush->resize(_context->brush->size() + size2d_t(1, 0));
  }
  else if (key == KEY_Q)
  {
    if (_context->brush->width() > 1)
      _context->brush->resize(_context->brush->size() + size2d_t(-1, 0));
  }
  else if (key == KEY_S)
  {
    _context->brush->resize(_context->brush->size() + size2d_t(0, 1));
  }
  else if (key == KEY_A)
  {
    if (_context->brush->height() > 1)
      _context->brush->resize(_context->brush->size() + size2d_t(0, -1));
  }
  else if (key == KEY_R)
  {
    _context->model->addLayerOnTop();
  }
  else if (key == KEY_F)
  {
    _context->renderer->resetCamera(model);
  }
  else if (key == KEY_C)
  {
    _context->model->clear();
  }


  //TODO: check validity
  else if (key == KEY_UP)
    _context->model->shift(Direction::North);
  else if (key == KEY_RIGHT)
    _context->model->shift(Direction::East);
  else if (key == KEY_DOWN)
    _context->model->shift(Direction::South);
  else if (key == KEY_LEFT)
    _context->model->shift(Direction::West);


  else if (key >= KEY_ZERO && key <= KEY_NINE)
  {
    int index = (key == KEY_ZERO) ? 9 : (key - KEY_ONE);
    auto it = _context->data->colors.begin();
    while (index > 0)
    {
      ++it;
      --index;
    }

    _context->brush->dye(&it->second);
  }

}
