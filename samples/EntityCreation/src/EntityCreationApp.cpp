#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

#include "entityx/System.h"
#include "soso/Transform.h"
#include "cinder/Perlin.h"
#include "cinder/Rand.h"

#include "Components.h"
#include "Systems.h"
#include "soso/Expires.h"
#include "soso/ExpiresSystem.h"
#include "soso/Behavior.h"
#include "soso/BehaviorSystem.h"
#include "Behaviors.h"

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace soso;

// Wraps positions to stay within a rectangle.
const auto borderWrap = createWrapFunction(Rectf(0, 0, 660, 500));

///
/// @file EntityCreationApp demonstrates creation of entities and some of the uses of components and systems.
/// This application, as a teaching tool, is much more heavily commented than a production codebase.
///
class EntityCreationApp : public App {
public:
  EntityCreationApp();

  void setup() override;
  void mouseDown(MouseEvent event) override;
  void keyDown(KeyEvent event) override;
  void update() override;
  void draw() override;

  entityx::Entity createDot(const ci::vec2 &position, const ci::vec2 &velocity, float diameter);

private:
  entityx::EventManager   events;
  entityx::EntityManager entities;
  entityx::SystemManager systems;
  uint32_t               num_dots = 0;
  static const uint32_t   max_dots = 1024;

  bool                   do_clear = true;
};

// Initialize our EntityManager and SystemManager to know about events and each other.
EntityCreationApp::EntityCreationApp()
: entities(events),
  systems(entities, events)
{}

void EntityCreationApp::setup()
{
  // Create our systems for controlling entity behavior.
  // We also use free functions for entity behavior, which don't need to be instantiated.
  systems.add<ExpiresSystem>();
  systems.add<BehaviorSystem>(entities);
  systems.configure();

  // Create an initial dot on screen
  createDot(getWindowCenter(), vec2(1, 0), 36.0f);
}

entityx::Entity EntityCreationApp::createDot(const ci::vec2 &position, const ci::vec2 &velocity, float diameter)
{
  // Keep track of the number of dots created so we don't have way too many.
  if (num_dots > max_dots) {
    return entityx::Entity(); // return an invalid entity
  }
  num_dots += 1;

  const auto dir = ([] (const vec2 &vel) {
    auto n = glm::normalize(vel);
    if (glm::any(glm::isnan(n))) {
      return randVec2();
    }
    return n;
  }(velocity));

  // Create an entity, managed by `entities`.
  auto dot = entities.create();
  // Assign the components we care about
  dot.assign<Position>(position);
  dot.assign<Circle>(diameter);
  dot.assign<Motion>(dir, glm::clamp(length(velocity) * 40.0f, 1.0f, 600.0f));
  // Assign an Expires component and store a handle to it in `exp`
  auto exp = dot.assign<Expires>(randFloat(4.0f, 6.0f));

  // Set a function to call when the Expires component runs out of time//
  // Called last_wish, since it happens just before the entity is destroyed.
  exp->last_wish = [this, diameter, velocity] (entityx::Entity e) {
    num_dots -= 1;

    if (diameter > 8.0f) {
      // If we weren't too small, create some smaller dots where we were destroyed.
      auto pos = e.component<Position>()->position;
      auto vel = glm::rotate<float>(velocity, M_PI / 2);

      createDot(pos, vel, diameter * 0.66f);
      createDot(pos, - vel, diameter * 0.66f);
    }
  };

  return dot;
}

void EntityCreationApp::mouseDown(MouseEvent event)
{
  // Create an entity that tracks the mouse.
  auto mouse_entity = entities.create();
  mouse_entity.assign<Position>(event.getPos());
  mouse_entity.assign<Circle>(49.0f, Color::gray(0.5f));

  // Assign a drag behavior component.
  auto tracker = assignBehavior<DragTracker>(mouse_entity);

  // When drag ends, run our lambda to create a new dot.
  tracker->setMouseUpCallback([this, mouse_entity] (const vec2 &position, const vec2 &previous) mutable {
    auto dir = position - previous;

    createDot(position, dir, 49.0f);
    mouse_entity.destroy();
  });
}

void EntityCreationApp::keyDown(KeyEvent event)
{
  if (event.getCode() == KeyEvent::KEY_c) {
    do_clear = ! do_clear;
  }
}

void EntityCreationApp::update()
{
  auto dt = 1.0 / 60.0;

  // Update all of our systems.
  systems.update<ExpiresSystem>(dt);
  systems.update<BehaviorSystem>(dt);

  // Apply functions to the entities. See Systems.h for these function definitions.
  slowDownWithAge(entities);
  applyMotion(entities, dt);
  borderWrap(entities);

  if (! do_clear) {
    fadeWithAge(entities);
  }
}

void EntityCreationApp::draw()
{
  if (do_clear) {
    gl::clear( Color( 0, 0, 0 ) );
  }
  gl::disableDepthRead();
  gl::setMatricesWindowPersp(getWindowSize());

  // Below, we loop through and draw every entity that has both a position and a circle component.
  // This is a bit more interesting than having a custom draw function for every entity,
  // because it allows us to do smart things based on what we know about our world.
  entityx::ComponentHandle<Circle>    ch;
  entityx::ComponentHandle<Position>  ph;

  gl::ScopedColor color(Color::white());
  for (auto __unused e : entities.entities_with_components(ch, ph)) {
    gl::color(ch->color);
    gl::drawSolidCircle(ph->position, ch->radius());
  }
}

CINDER_APP( EntityCreationApp, RendererGl(RendererGl::Options().msaa(4)) )
