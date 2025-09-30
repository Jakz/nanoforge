#include "input.h"

#include "model/model.h"
#include "renderer.h"
#include "context.h"

extern Data data;


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
    float y = (gfx::Renderer::MOCK_LAYER_SIZE * Data::Constants::LAYER2D_CELL_SIZE.height) * i + (Data::Constants::LAYER2D_SPACING * i);
    rect bounds = rect(Data::Constants::LAYER2D_BASE.x, Data::Constants::LAYER2D_BASE.y + y, gfx::Renderer::MOCK_LAYER_SIZE * Data::Constants::LAYER2D_CELL_SIZE.width, gfx::Renderer::MOCK_LAYER_SIZE * Data::Constants::LAYER2D_CELL_SIZE.height);

    /* if mouse is inside 2d layer grid */
    if (bounds.CheckCollision(position))
    {
      auto relative = position - bounds.Origin();
      coord2d_t cell = coord2d_t(relative.x / Data::Constants::LAYER2D_CELL_SIZE.width, relative.y / Data::Constants::LAYER2D_CELL_SIZE.height);
      _hover = coord3d_t(cell, model->lastLayerIndex() - i);
      any = true;
      break;
    }
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
        mouseUp(static_cast<MouseButton>(i));
    }
  }
  _mouseState = newState;
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
      piece.moveAt(_hover->xy());
      model->addPiece(_hover->z, piece);
    }
  }
  else if (button == MouseButton::Right)
  {
    _context->brush->swapSize();
  }
}

void InputHandler::mouseUp(MouseButton button)
{

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
    auto it = data.colors.begin();
    while (index > 0)
    {
      ++it;
      --index;
    }

    _context->brush->dye(&it->second);
  }

}
