#include "undo.h"

using namespace undo;

void ShiftAction::undo(nb::Model* model)
{
  model->shift(-_delta);
}

void ShiftAction::redo(nb::Model* model)
{
  model->shift(_delta);
}

void History::execute(Action* action)
{
  action->redo(_model);
  _actions.push_back(std::unique_ptr<Action>(action));
}

void History::undoLast()
{
  if (_actions.empty())
    return;

  auto action = std::move(_actions.back());
  _actions.pop_back();
  action->undo(_model);
}