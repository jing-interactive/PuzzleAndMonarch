﻿#pragma once

//
// UI Widget
//

#include "UIWidgetBase.hpp"


namespace ngs { namespace UI {

// TIPS 自分自身を引数に取る関数があるので先行宣言が必要
class Widget;
using WidgetPtr = std::shared_ptr<Widget>;


class Widget
  : private boost::noncopyable
{

  
public:
  Widget(const ci::Rectf& rect) noexcept
    : rect_(rect)
  {
#if defined (DEBUG)
    // 枠線のために適当に色を決める
    disp_color_ = ci::hsvToRgb(glm::vec3(ci::randFloat(), 1.0f, 1.0f));
#endif
  }

  ~Widget() = default;


  const std::string& getIdentifier() const noexcept
  {
    return identifier_;
  }

  bool hasIdentifier() const noexcept
  {
    return !identifier_.empty();
  }

  const std::string& getEvent() const noexcept
  {
    return event_;
  }

  bool hasEvent() const noexcept
  {
    return has_event_ && active_;
  }
  
  const std::string& getSe() const noexcept
  {
    return se_;
  }

  void setSe(const std::string& se)
  {
    se_ = se;
  }

  bool hasSe() const noexcept
  {
    return !se_.empty();
  }

  // Touch-Moveでも反応する
  bool reactMoveEvent() const
  {
    return has_event_ && move_event_;
  }

  bool hasChild() const noexcept
  {
    return !children_.empty();
  }

  size_t getChildNum() const
  {
    return children_.size();
  }

  void addChildren(const WidgetPtr& widget) noexcept
  {
    children_.push_back(widget);
  }

  const std::vector<WidgetPtr>& getChildren() const noexcept
  {
    return children_;
  }

  void setWidgetBase(std::unique_ptr<UI::WidgetBase>&& base) noexcept
  {
    widget_base_ = std::move(base);
  }

  // FIXME 直前の描画結果から判定している
  bool contains(const glm::vec2& point) const noexcept
  {
    return parent_enable_
           && enable_
           && disp_rect_.contains(point);
  }

  // 画面に表示するかどうかの制御
  //   全ての子供にも影響
  void enable(bool enable = true) noexcept
  {
    enable_ = enable;
    // 再帰的に設定

    if (children_.empty()) return;

    for (const auto& child : children_)
    {
      child->parentEnable(enable);
    }
  }

  bool isEnable() const noexcept
  {
    return enable_;
  }

  // eventを処理するか
  void active(bool active = true) noexcept
  {
    active_ = active;
  }

  bool isActive() const noexcept
  {
    return active_;
  }


  // パラメーター設定
  void setParam(const std::string& name, const boost::any& value) noexcept
  {
    // UI::Widget内
    std::map<std::string, std::function<void (const boost::any& v)>> tbl = {
      { "rect", [this](const boost::any& v) noexcept
                {
                  rect_ = boost::any_cast<const ci::Rectf&>(v);
                }
      },
      { "pivot", [this](const boost::any& v) noexcept
                 {
                   pivot_ = boost::any_cast<const glm::vec2&>(v);
                 }
      },
      { "anchor_min", [this](const boost::any& v) noexcept
                      {
                        anchor_min_ = boost::any_cast<const glm::vec2&>(v);
                      }
      },
      { "anchor_max", [this](const boost::any& v) noexcept
                      {
                        anchor_max_ = boost::any_cast<const glm::vec2&>(v);
                      }
      },
      { "offset", [this](const boost::any& v) noexcept
                  {
                    offset_ = boost::any_cast<const glm::vec2&>(v);
                  }
      },
      { "scale", [this](const boost::any& v) noexcept
                 {
                   scale_ = boost::any_cast<const glm::vec2&>(v);
                 }
      },
      { "alpha", [this](const boost::any& v) noexcept
                 {
                   alpha_ = boost::any_cast<float>(v);
                 }
      }
    };

    if (tbl.count(name))
    {
      // TIPS コンテナを関数ポインタとして利用
      tbl.at(name)(value);
    }
    else
    {
      //
      widget_base_->setParam(name, value);
    }
  }

  // パラメータ取得(Pointerで返却する)
  boost::any getParam(const std::string& name) noexcept
  {
    // UI::Widget内
    std::map<std::string, std::function<boost::any ()>> tbl = {
      { "rect", [this]() noexcept
                {
                  return &rect_;
                }
      },
      { "pivot", [this]() noexcept
                 {
                   return &pivot_;
                 }
      },
      { "anchor_min", [this]() noexcept
                      {
                        return &anchor_min_;
                      }
      },
      { "anchor_max", [this]() noexcept
                      {
                        return &anchor_max_;
                      }
      },
      { "offset", [this]() noexcept
                  {
                    return &offset_;
                  }
      },
      { "scale", [this]() noexcept
                 {
                   return &scale_;
                 }
      },
      { "alpha", [this]() noexcept
                 {
                   return &alpha_;
                 }
      }
    };

    if (tbl.count(name))
    {
      // TIPS コンテナを関数ポインタとして利用
      return tbl.at(name)();
    }
    else
    {
      return widget_base_->getParam(name);
    }
  }


  void draw(const ci::Rectf& parent_rect, UI::Drawer& drawer, float parent_alpha) noexcept
  {
    if (!enable_) return;

    auto alpha = parent_alpha * alpha_;
    disp_rect_ = calcRect(parent_rect, scale_);
    widget_base_->draw(disp_rect_, drawer, alpha);

    for (const auto& child : children_)
    {
      child->draw(disp_rect_, drawer, alpha);
    }
  }

#if defined (DEBUG)
  // デバッグ用にRectを描画
  void debugDraw() const noexcept
  {
    if (!enable_) return;

    ci::gl::color(disp_color_);
    ci::gl::drawStrokedRect(disp_rect_);

    for (const auto& child : children_)
    {
      child->debugDraw();
    }
  }
#endif


  // パラメーターから生成
  static WidgetPtr createFromParams(const ci::JsonTree& params, bool safe_area) noexcept
  {
    auto rect   = Json::getRect<float>(params["rect"]);
    auto widget = std::make_shared<UI::Widget>(rect);

    if (params.hasChild("identifier"))
    {
      widget->identifier_ = params.getValueForKey<std::string>("identifier");
    }

    widget->enable_ = Json::getValue(params, "enable", true);
    widget->active_ = Json::getValue(params, "active", true);
    widget->alpha_  = Json::getValue(params, "alpha",  1.0f);

    if (safe_area && params.hasChild("anchor_safe"))
    {
      // SafeAreaがある場合は専用アンカー
      const auto& p = params["anchor_safe"];
      widget->anchor_min_ = Json::getVec<glm::vec2>(p[0]);
      widget->anchor_max_ = Json::getVec<glm::vec2>(p[1]);
    }
    else if (params.hasChild("anchor"))
    {
      const auto& p = params["anchor"];
      widget->anchor_min_ = Json::getVec<glm::vec2>(p[0]);
      widget->anchor_max_ = Json::getVec<glm::vec2>(p[1]);
    }

    widget->scale_  = Json::getVec(params, "scale",  glm::vec2(1));
    widget->pivot_  = Json::getVec(params, "pivot",  glm::vec2(0.5));
    widget->offset_ = Json::getVec(params, "offset", glm::vec2());

    if (params.hasChild("event"))
    {
      widget->event_     = params.getValueForKey<std::string>("event");
      widget->has_event_ = true;
    }
    widget->move_event_ = Json::getValue(params, "move_event", false);

    if (params.hasChild("se"))
    {
      widget->se_ = params.getValueForKey<std::string>("se");
    }

    return widget;
  }


  // 非表示なWidgetの検査
  static void checkInactiveWidget(const WidgetPtr& widget) noexcept
  {
    if (!widget->isEnable())
    {
      widget->enable(false);
    }

    if (widget->children_.empty()) return;

    for (const auto& child : widget->children_)
    {
      checkInactiveWidget(child);
    }
  }


private:
  // 親の情報から自分の位置、サイズを計算
  ci::Rectf calcRect(const ci::Rectf& parent_rect, const glm::vec2& scale) const noexcept
  {
    auto rect = rect_.getOffset(offset_);

    glm::vec2 parent_size = parent_rect.getSize();

    // 親のサイズとアンカーから左下・右上の座標を計算
    glm::vec2 anchor_min = parent_size * anchor_min_;
    glm::vec2 anchor_max = parent_size * anchor_max_;

    // 相対座標(スケーリング抜き)
    glm::vec2 pos  = rect.getUpperLeft()  + anchor_min;
    glm::vec2 size = rect.getLowerRight() + anchor_max - pos;

    // pivotを考慮したスケーリング
    glm::vec2 d = size * pivot_;
    pos -= d * scale - d;
    size *= scale;

    glm::vec2 parent_pos = parent_rect.getUpperLeft();
    return ci::Rectf(pos + parent_pos, pos + size + parent_pos);
  }
  
  void parentEnable(bool enable) noexcept
  {
    parent_enable_ = enable;
  }


  // メンバ変数
  std::string identifier_;
  bool enable_;
  // 親も有効
  bool parent_enable_ = true;

  bool active_ = true;

  ci::Rectf rect_;
  glm::vec2 offset_;

  // スケーリングの中心(normalized)
  glm::vec2 pivot_;

  // 親のサイズの影響力(normalized)
  glm::vec2 anchor_min_ = { 0.5f, 0.5f };
  glm::vec2 anchor_max_ = { 0.5f, 0.5f };

  glm::vec2 scale_;

  bool has_event_ = false;
  std::string event_;

  bool move_event_;

  std::string se_;

  float alpha_;

  // 描画用クラス
  std::unique_ptr<UI::WidgetBase> widget_base_;

  std::vector<WidgetPtr> children_;

  // 画面上のサイズ
  ci::Rectf disp_rect_;

#if defined (DEBUG)
  ci::Color disp_color_;
#endif

};

} }
