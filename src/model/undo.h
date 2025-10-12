#pragma once

#include "model/common.h"
#include "model/model.h"

#include <vector>
#include <memory>

namespace undo
{
  class Action
  {
  protected:

  public: 
    Action() { }
    virtual ~Action() = default;
    virtual void undo(nb::Model* model) = 0;
    virtual void redo(nb::Model* model) = 0;
  };

  class ShiftAction : public Action
  {
  protected:
    coord2d_t _delta;

  public:
    ShiftAction(const coord2d_t& delta) : _delta(delta) { }
    virtual void undo(nb::Model* model) override;
    virtual void redo(nb::Model* model) override;
  };
  
  class History
  {
  protected:
    nb::Model* _model;
    std::vector<std::unique_ptr<Action>> _actions;

  public:
    History(nb::Model* model) : _model(model) { }

    void execute(Action* action);
    void undoLast();

    bool canUndo() const { return !_actions.empty(); }
  };
}