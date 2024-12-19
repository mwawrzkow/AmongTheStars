#include "fmt/base.h"
#include <SFML/Graphics.hpp>
#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/ConvexShape.hpp>
#include <SFML/Graphics/Drawable.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/WindowStyle.hpp>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fmt/format.h>
#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <thread>

int WINDOW_WIDTH = 1980;
int WINDOW_HEIGHT = 1080;
float VP_WIDTH = 0;
float VP_HEIGHT = 0;
const int MARGIN = 100;
template <typename T> T VecLength(sf::Vector2<T> vec) {
  return std::sqrt(vec.x * vec.x + vec.y * vec.y);
}

template <typename T> sf::Vector2<T> &normalize(sf::Vector2<T> &vec) {
  auto len = VecLength(vec);
  vec /= len;
  return vec;
}

template <typename T> T PointLen(sf::Vector2<T> p0, sf::Vector2<T> p1) {
  sf::Vector2<T> vec = {p0.x - p1.x, p0.y - p1.y};
  return VecLength<T>(vec);
}

class TextureProvider {
public:
  static sf::Texture &getTexture(std::filesystem::path path) {
    auto texture = textures.find(path.c_str());
    if (texture != textures.end()) {
      return (*texture).second;
    }
    textures[path] = generateTexture(path);
    return getTexture(path);
  }

  static std::shared_ptr<sf::Font> getDefaultFont() {
    std::cout << "Looking for default font" << std::endl;
    if (defaultFont)
      return defaultFont;
    defaultFont = std::make_shared<sf::Font>();
    defaultFont->loadFromFile(
        std::filesystem::absolute("./fonts/Audiowide-Regular.ttf"));
    std::cout << "default Font loaded" << std::endl;
    return getDefaultFont();
  }

private:
  static sf::Texture generateTexture(std::filesystem::path path) {
    sf::Texture tmp;
    tmp.loadFromFile(std::filesystem::absolute(path));
    tmp.setSmooth(true);
    return tmp;
  }

  static std::map<std::string, sf::Texture> textures;
  static std::shared_ptr<sf::Font> defaultFont;
};

std::map<std::string, sf::Texture> TextureProvider::textures = {};
std::shared_ptr<sf::Font> TextureProvider::defaultFont = nullptr;

class Drawable {
public:
  virtual void draw(sf::RenderWindow &rw) = 0;
};

class GameObject : public Drawable {
public:
  GameObject(std::filesystem::path texture, sf::Vector2f pos)
      : sprite(TextureProvider::getTexture(texture)) {
    sprite.setPosition(pos);
  }

  void setDefaultRect(sf::IntRect rec) {
    sprite.setTextureRect(rec);
    sprite.setOrigin({static_cast<float>(rec.width) / 2.0f,
                      static_cast<float>(rec.height) / 2.0f});
  }

  const bool intersects(const GameObject &go) const {
    return sprite.getGlobalBounds().intersects(go.sprite.getGlobalBounds());
  }

  virtual void onColision(std::weak_ptr<GameObject> obj, float dt) {}
  virtual void draw(sf::RenderWindow &rw) override { rw.draw(sprite); }

  void setPosition(sf::Vector2f pos) { sprite.setPosition(pos); }

  const sf::Sprite &getSprite() const { return sprite; }

protected:
  sf::Sprite sprite;
};

struct inputs {
  bool W = false;
  bool S = false;
  bool A = false;
  bool D = false;
  bool SPACE = false;
};

class Tickable {
public:
  virtual void tick(float dt) = 0;
  virtual void tick(float dt, const inputs &input) { tick(dt); }
};

class Movable {
public:
  Movable() = default;
  void setPos(float x, float y) {
    pos.x = x;
    pos.y = y;
  }
  void addAcc(float x, float y) {
    acc.x += x;
    acc.y += y;
  }
  void addAcc(sf::Vector2f app) { acc += app; }
  void PhysicsTick(float dt, float maxSpeed = 0) {
    if (maxSpeed != 0)
      if (VecLength(acc) > maxSpeed)
        normalize(acc) *= maxSpeed;
    pos += acc * dt;
  }

  const sf::Vector2f getInverseAcc() const { return -acc; }
  const sf::Vector2f &getPos() const { return pos; }

protected:
  sf::Vector2f pos;
  sf::Vector2f acc = {0, 0};
};

class ProgressBar : public Drawable, public Tickable, public Movable {
public:
  ProgressBar(float val = 0, float maxVal = 100, float width = 256,
              float height = 32)
      : maxVal(maxVal), val(val) {
    SliderWrapper.setSize({width, height});
  }
  void updateValue(float val) { this->val = val; }
  virtual void draw(sf::RenderWindow &rw) override {
    rw.draw(SliderWrapper);
    rw.draw(ValueWrapper);
  }
  virtual void tick(float dt) override {
    auto pos = getPos();
    SliderWrapper.setPosition(pos);
    pos.x += 5;
    pos.y += 5;
    ValueWrapper.setPosition(pos);
    auto maxSize = SliderWrapper.getSize();
    maxSize.x -= 10;
    maxSize.y -= 10;
    maxSize.x = maxSize.x * (val / maxVal);
    ValueWrapper.setSize({maxSize.x, maxSize.y});
  }
  void setFillColor(sf::Color color) { ValueWrapper.setFillColor(color); }
  void setBackgroundColor(sf::Color color) {
    SliderWrapper.setFillColor(color);
  }

private:
  float maxVal;
  float val;
  sf::RectangleShape SliderWrapper;
  sf::RectangleShape ValueWrapper;
};

class Text : public Movable, public Tickable, public Drawable {
public:
  Text() : textCallback(nullptr) {
    txt.setFont(*TextureProvider::getDefaultFont());
    txt.setCharacterSize(12);
    txt.setFillColor(sf::Color::White);
  }
  Text(std::shared_ptr<std::function<std::string()>> callback)
      : textCallback(callback) {
    txt.setFont(*TextureProvider::getDefaultFont());
    txt.setCharacterSize(12);
    txt.setFillColor(sf::Color::White);
  }
  void setText(std::string text) { txt.setString(text); }
  void setColor(sf::Color col) { txt.setFillColor(col); }
  void setFontSize(int fontSize) { txt.setCharacterSize(fontSize); }
  virtual void tick(float dt) override {
    txt.setPosition(getPos());
    if (textCallback) {
      std::string data = (*textCallback)();
      txt.setString(data);
    }
  }
  virtual void draw(sf::RenderWindow &rw) override { rw.draw(txt); }

  sf::Text &getUnderlayingType() { return txt; }

private:
  sf::Text txt;
  std::shared_ptr<std::function<std::string()>> textCallback;
};

class Arrow : public GameObject, public Movable, public Tickable {
public:
  Arrow() : GameObject("./assets/Arrow.png", {0, 0}) {
    sprite.setScale(0.25, 0.25);
    auto size = sprite.getTexture()->getSize();
    sprite.setOrigin(size.x / 2, size.y / 2);
  }
  virtual void tick(float dt) override {
    calculateAngle();
    sf::Vector2f newPos = {0, -100};
    float anglerad = -angle * (M_PI / 180.0f);
    newPos.x = newPos.x * cos(anglerad) - newPos.y * sin(anglerad);
    newPos.y = newPos.y * cos(anglerad) + newPos.x * sin(anglerad);
    newPos += origin;
    setPosition(newPos);
    // idk why
    sprite.setRotation(-angle);
  }

  void calculateAngle() {
    auto t = target.lock()->getPos();

    // Calculate angle using atan2 (result is in radians, range: -π to +π)
    angle = std::atan2(origin.x - t.x, origin.y - t.y);

    // Convert to degrees and normalize to [0, 360]
    angle = angle * (180.0f / M_PI); // Convert radians to degrees
    if (angle < 0) {
      angle += 360.0f; // Ensure non-negative angle
    }
  }

  void setAngle(float angle) { this->angle = angle; }
  void setOrigin(sf::Vector2f origin) { this->origin = origin; }
  void setTarget(std::weak_ptr<Movable> target) { this->target = target; }

protected:
  sf::Vector2f origin = {0, 0};
  float angle = 0;
  std::weak_ptr<Movable> target;
};
inline float PlayerGameScore = 0;
class Player : public GameObject, public Movable, public Tickable {
public:
  Player(std::filesystem::path texture, sf::Vector2f pos)
      : GameObject(texture, pos),
        Position(std::make_shared<std::function<std::string()>>([this]() {
          auto pos = getPos();
          std::string data = fmt::format("X: {:.0f}, Y: {:.0f}", pos.x, pos.y);
          return data;
        })),
        Acceleration(std::make_shared<std::function<std::string()>>([this]() {
          std::string data =
              fmt::format("X: {:.2f} m/s2, Y: {:.2f} m/s2", acc.x, acc.y);
          return data;
        })),
        Points(std::make_shared<std::function<std::string()>>([this]() {
          std::string data =
              fmt::format("Score: {:.0f}", points + PlayerGameScore);
          return data;
        })),
        PlayerShipStatus(
            std::make_shared<std::function<std::string()>>([this]() {
              std::string data = fmt::format(
                  "Boarding fly around the ship for {:.0f}", 30.0f - dtShip);
              return data;
            })) {
    PlayerShipStatus.setFontSize(32);
    setPos(pos.x, pos.y);
    oxygenSlider.setFillColor(sf::Color::Blue);
    oxygenSlider.setBackgroundColor(sf::Color(115, 115, 115));
    oxygenSlider.setPos(800, 800);
    fuelSlider.setFillColor(sf::Color::Yellow);
    fuelSlider.setBackgroundColor(sf::Color(115, 115, 115));
    fuelSlider.setPos(800, 800);
    setDefaultRect({0, 0, 52, 89});
sprite.scale(0.5,0.5);
  }
  void Kill() {
    using namespace std::literals;
    if (creationTime - std::chrono::system_clock::now() > 15s)
      return;
    dead = true;
    fuel = 0;
    oxygen = 0;
    addAcc(getInverseAcc());
  }

  bool isDead() { return oxygen <= 0; }
  bool isWon() { return dtShip > 30; }

  void addResources(float fuel, float oxygen) {
    if (dead)
      return;
    this->fuel += fuel;
    this->oxygen += oxygen;
    if (this->fuel > 100.f)
      this->fuel = 100;
    if (this->oxygen > 100.f)
      this->oxygen = 100;
  }

  void setTarget(std::weak_ptr<Movable> target) {
    this->target = target;
    arrow.setTarget(target);
  }
  virtual void tick(float dt) override {
    internalTick(dt);
    PhysicsTick(dt, maxSpeed);
    oxygen -= dt * 1;
    if (fuel == 0)
      oxygen -= dt * 4;
    if (oxygen <= 0)
      oxygen = 0;
    updateUIElements();
  }

  virtual void tick(float dt, const inputs &input) override {
    if (dtShip > 30)
      fmt::println("Player WON!");

    tick(dt);
    points = fuel * 0.2f + oxygen * 0.5f;
    if (oxygen == 0)
      return;
    sf::Vector2f accAppend = {0, 0};
    if (input.W)
      accAppend.y -= accTickSpeed * dt;
    if (input.S)
      accAppend.y += accTickSpeed * dt;
    if (input.D)
      accAppend.x += accTickSpeed * dt;
    if (input.A)
      accAppend.x -= accTickSpeed * dt;
    if (input.SPACE)
      accAppend = getInverseAcc() * dt;
    if (VecLength(accAppend) != 0 && fuel != 0)
      fuel -= 5.f * dt;

    if (fuel <= 0) {
      accAppend = {0, 0};
      fuel = 0;
    }
    addAcc(accAppend);
  }
  void updatePlayerGlobalScore() { PlayerGameScore += points; }
  void updateTimer(float dt) { dtShip += dt; }
  void zeroPlayerTimer() { dtShip = 0; }
  Text &getPosition() { return Position; }

  Text &getAcc() { return Acceleration; };

  ProgressBar &getOxygenLevelSlider() { return oxygenSlider; }
  ProgressBar &getFuelLevelSlider() { return fuelSlider; }
  virtual void draw(sf::RenderWindow &rw) override {
    if (!dead) {
      oxygenSlider.draw(rw);
      fuelSlider.draw(rw);
      Position.draw(rw);
      Acceleration.draw(rw);
      Points.draw(rw);
      if (dtShip > 0)
        PlayerShipStatus.draw(rw);
      if (PointLen(getPos(), target.lock()->getPos()) > 200.0f)
        arrow.draw(rw);
    }
    rw.draw(this->sprite);
  }

private:
  void updateUIElements() {
    {
      auto pos = getPos();
      auto offsetRight = VP_WIDTH / 2.0f;
      auto offsetBottom = VP_HEIGHT / 2.0f;
      pos.x -= offsetRight - MARGIN;
      pos.y += offsetBottom - 32;
      oxygenSlider.setPos(pos.x, pos.y);
    }
    {
      auto pos = getPos();
      auto offsetRight = VP_WIDTH / 2.0f;
      auto offsetBottom = VP_HEIGHT / 2.0f;
      pos.x += offsetRight - 256 - MARGIN;
      pos.y += offsetBottom - 32;

      fuelSlider.setPos(pos.x, pos.y);
    }
    {
      auto pos = getPos();
      auto offsetRight = VP_WIDTH / 2.0f;
      auto offsetBottom = VP_HEIGHT / 2.0f;
      Points.setPos(pos.x - offsetRight, pos.y - offsetBottom);
    }
    {
      auto pos = getPos();
      auto &text = PlayerShipStatus.getUnderlayingType();
      auto textRect = text.getLocalBounds();
      text.setOrigin(textRect.left + textRect.width / 2.0f,
                     textRect.top + textRect.height / 2.0f);
      PlayerShipStatus.setPos(pos.x, pos.y - VP_HEIGHT / 4);
    }
    arrow.setOrigin(getPos());
    setPosition(getPos());
    auto realPos = getPos();
    realPos.x += 50;
    Position.setPos(realPos.x, realPos.y);
    Acceleration.setPos(realPos.x, realPos.y + 12);

    oxygenSlider.updateValue(oxygen);
    fuelSlider.updateValue(fuel);
  }

  void internalTick(float dt) {
    arrow.tick(dt);
    Position.tick(dt);
    Points.tick(dt);
    PlayerShipStatus.tick(dt);
    Acceleration.tick(dt);
    oxygenSlider.tick(dt);
    fuelSlider.tick(dt);
  }

protected:
  bool dead = false;
  float maxSpeed = 500.f;
  float accTickSpeed = 100.f;
  float fuel = 100.f;
  float oxygen = 100.0f;
  float points = 0;
  float dtShip = 0.0f;
  Text Position, Acceleration, Points, PlayerShipStatus;
  ProgressBar oxygenSlider, fuelSlider;
  Arrow arrow;
  std::weak_ptr<Movable> target;
  std::chrono::time_point<std::chrono::system_clock> creationTime =
      std::chrono::system_clock::now();
};

class Spaceship : public Tickable, public Movable, public GameObject {
public:
  Spaceship(float minDistanceFromPlayer)
      : GameObject("./assets/spaceship_scaled.png", {0, 0}) {
    sf::Vector2f pos{static_cast<float>(rand() % 2048) - 1024 / 2,
                     static_cast<float>(rand() % 2048) - 1024 / 2};
    auto randomOffset = [](float min, float max) {
      return min + static_cast<float>(rand()) /
                       (static_cast<float>(RAND_MAX / (max - min)));
    };

    while (true) {
      sf::Vector2f pos{randomOffset(-2048.0f, 2048.0f),
                       randomOffset(-2048.0f, 2048.0f)};

      // Ensure the spaceship is sufficiently far from the origin (player's
      // initial position)
      if (VecLength(pos) >= minDistanceFromPlayer) {
        setPosition(pos);
        setPos(pos.x, pos.y);
        break;
      }
    }

    auto rec = sprite.getTexture()->getSize();
    setDefaultRect({0, 0, static_cast<int>(rec.x), static_cast<int>(rec.y)});
  }
  virtual void tick(float dt) override {
    if (!PlayerRef)
      return;
    if (!intersects(*PlayerRef))
      PlayerRef->zeroPlayerTimer();
    if (intersects(*PlayerRef)) {
      PlayerRef->updateTimer(dt);
      PlayerRef->addResources(2.0f * dt, 10 * dt);
    }
  }
  virtual void onColision(std::weak_ptr<GameObject> obj, float dt) override {
    auto s_obj = obj.lock();
    if (Player *player = dynamic_cast<Player *>(s_obj.get())) {
      if (intersects(*player)) {
        fmt::println("Player Found Ship!");
        PlayerRef = player;
      }
    }
  }

private:
  Player *PlayerRef = nullptr;
};

class Asteroid : public GameObject, public Movable, public Tickable {
public:
  Asteroid() : GameObject("./assets/asteroid.png", {0, 0}) {
    auto maxSpeed = 50;
    auto x = rand() % maxSpeed;
    auto xSign = rand() % 2;
    auto y = rand() % maxSpeed;
    auto ySign = rand() % 2;
    x *= (xSign % 2 ? 1 : -1);
    y *= (ySign % 2 ? 1 : -1);
    // addAcc(x, y);
    sprite.setScale(0.5, 0.5);
    auto max = 2048 * 4;
    sf::Vector2f pos = {static_cast<float>(rand() % max) - max / 2,
                        static_cast<float>(rand() % max) - max / 2};
    if (std::abs(pos.x) < 150)
      pos.x += xSign * 150;
    if (std::abs(pos.y) < 150)
      pos.y += ySign * 150;
    setPos(pos.x, pos.y);
  }
  virtual void tick(float dt) override {
    auto lastPos = getPos();
    PhysicsTick(dt);
    setPosition(getPos());
    auto offset = getPos() - lastPos;
    if (PlayerRef)
      PlayerRef->setPos(offset.x + PlayerRef->getPos().x,
                        offset.y + PlayerRef->getPos().y);
    colided = false;
  }

  virtual void onColision(std::weak_ptr<GameObject> obj, float dt) override {
    auto s_obj = obj.lock();
    if (Player *player = dynamic_cast<Player *>(s_obj.get())) {
      if (!isPlayerAttached)
        if (intersects(*player)) {
          fmt::println("Player Found Asteroid, will die!!");
          player->Kill();
          PlayerRef = player;
          isPlayerAttached = true;
        }
    }
    if (Asteroid *asteroid = dynamic_cast<Asteroid *>(s_obj.get())) {
      if (!colided && !asteroid->colided && intersects(*asteroid)) {
        // Get positions
        sf::Vector2f pos1 = getPos();
        sf::Vector2f pos2 = asteroid->getPos();

        // Get velocities
        sf::Vector2f vel1 = acc;
        sf::Vector2f vel2 = asteroid->acc;

        // Collision normal
        sf::Vector2f collisionNormal = pos2 - pos1;
        normalize(collisionNormal);

        // Relative velocity
        sf::Vector2f relativeVelocity = vel2 - vel1;

        // Calculate velocity along the normal
        float velocityAlongNormal = relativeVelocity.x * collisionNormal.x +
                                    relativeVelocity.y * collisionNormal.y;

        // Ignore if velocities are separating
        if (velocityAlongNormal > 0) {
          return;
        }

        // Coefficient of restitution (elasticity, range: 0.0 to 1.0)
        float restitution = 0.8f;

        // Impulse scalar
        float impulseScalar = -(1.0f + restitution) * velocityAlongNormal;

        // Assuming equal mass for simplicity
        impulseScalar /= 2.0f;

        // Impulse vector
        sf::Vector2f impulse = impulseScalar * collisionNormal;

        // Apply impulse to both objects
        acc -= impulse;
        asteroid->addAcc(impulse);

        // Mark as collided
        colided = true;
        asteroid->colided = true;
      }
    }
  }
  ~Asteroid() { isPlayerAttached = false; }

private:
  Player *PlayerRef = nullptr;
  bool colided = false;
  static bool isPlayerAttached;
};
bool Asteroid::isPlayerAttached = false;

void mapByKeyCode(const sf::Event event, const bool defaultVal, inputs &input) {
  switch (event.key.code) {
  case sf::Keyboard::W:
    input.W = defaultVal;
    break;
  case sf::Keyboard::S:
    input.S = defaultVal;
    break;
  case sf::Keyboard::D:
    input.D = defaultVal;
    break;
  case sf::Keyboard::A:
    input.A = defaultVal;
    break;
  case sf::Keyboard::Space:
    input.SPACE = defaultVal;
    break;
  default:
    break;
  }
}

class Background : public Drawable {
public:
  Background(int amount, sf::Vector2i size, sf::Vector2i offset,
             sf::RenderWindow &window, int threadCount = 4)
      : textureSize(size) {
    renderTexture.create(size.x, size.y);
    renderTexture.clear(sf::Color::Black);

    std::atomic<int> progressCounter(0); // Atomic counter for progress
    std::vector<std::future<std::vector<sf::ConvexShape>>> futures;

    // Divide star generation work among threads
    int starsPerThread = amount / threadCount;

    for (int t = 0; t < threadCount; ++t) {
      futures.push_back(
          std::async(std::launch::async, [=, &progressCounter, this]() {
            auto stars = generateStars(starsPerThread, size);
            progressCounter++; // Increment progress
            return stars;
          }));
    }

    // Main thread: Monitor progress and render the progress bar
    int totalSteps = threadCount;
    while (progressCounter < totalSteps) {
      float progress = static_cast<float>(progressCounter) / totalSteps;
      drawProgressBar(window, progress);
      std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Small delay
    }

    // Collect stars from threads and draw them
    for (auto &future : futures) {
      auto stars = future.get();
      for (const auto &star : stars) {
        renderTexture.draw(star);
      }
    }

    // Finalize the texture
    renderTexture.display();
    backgroundSprite.setTexture(renderTexture.getTexture());
    backgroundSprite.setPosition(-offset.x, -offset.y);

    // Shader setup
    if (!shader.loadFromMemory(blur, sf::Shader::Fragment)) {
      throw std::runtime_error("Failed to load shader");
    }

    shader.setUniform("resolution", sf::Vector2f(textureSize.x, textureSize.y));
    shader.setUniform("glowRadius", 1000.0f); // Glow radius
    shader.setUniform("glowColor", sf::Glsl::Vec4(1.0, 1.0, 1.0, 0.5));
  }

  virtual void draw(sf::RenderWindow &rw) override {
    auto &view = rw.getView();
    auto viewSize = view.getSize();
    sf::Vector2f cameraCenter = view.getCenter();
    shader.setUniform("cameraCenter", cameraCenter); // Pass camera position
    shader.setUniform("viewSize", viewSize);         // Pass view size
    rw.draw(backgroundSprite, &shader);
  }

private:
  void drawProgressBar(sf::RenderWindow &window, float progress) {
    // Handle window events to keep the window responsive
    sf::Event ev;
    while (window.pollEvent(ev)) {
      if (ev.type == sf::Event::Closed) {
        window.close();
      }
    }

    // Background bar
    sf::RectangleShape barBackground({400.0f, 30.0f});
    barBackground.setFillColor(sf::Color(50, 50, 50)); // Dark gray
    barBackground.setPosition(600.0f, 500.0f);         // Center position

    // Progress bar (fill)
    sf::RectangleShape barFill({400.0f * progress, 30.0f});
    barFill.setFillColor(sf::Color(100, 250, 100)); // Green
    barFill.setPosition(600.0f, 500.0f); // Same position as background

    // Clear and draw
    window.clear(sf::Color::Black);
    window.draw(barBackground);
    window.draw(barFill);
    window.display();
  }
  std::vector<sf::ConvexShape> generateStars(int amount, sf::Vector2i size) {
    std::vector<sf::ConvexShape> stars;

    for (int i = 0; i < amount; ++i) {
      float x = static_cast<float>((std::rand() % size.x));
      float y = static_cast<float>((std::rand() % size.y));
      float outerRadius =
          static_cast<float>(3 + (std::rand() % 5)); // Random size
      float innerRadius = outerRadius / 2.5f;

      stars.push_back(createStar(x, y, outerRadius, innerRadius));
    }

    return stars;
  }

  sf::ConvexShape createStar(float x, float y, float radius, float innerRadius,
                             int points = 5) {
    sf::ConvexShape star(points * 2);

    const float angleStep = 2 * 3.14159265f / points;

    for (int i = 0; i < points * 2; ++i) {
      float radiusToUse = (i % 2 == 0) ? radius : innerRadius;
      float angle = i * (angleStep / 2);

      float vx = x + std::cos(angle) * radiusToUse;
      float vy = y + std::sin(angle) * radiusToUse;

      star.setPoint(i, sf::Vector2f(vx, vy));
    }

    star.setFillColor(getRandomStarColor());
    return star;
  }
  sf::Color getRandomStarColor() {
    int brightness =
        (std::rand() % 256); // Range: 200 to 255 for shades of white
    return sf::Color(brightness, brightness, brightness);
  }

  sf::RenderTexture renderTexture;
  sf::Sprite backgroundSprite;
  sf::Vector2i textureSize;
  sf::Shader shader;
  std::string blur = R"(uniform sampler2D texture;      // The input texture
uniform vec2 resolution;        // Resolution of the texture
uniform vec2 cameraCenter;      // Camera/view center
uniform vec2 viewSize;          // Camera/view size
uniform float glowRadius;       // Radius of the glow
uniform vec4 glowColor;         // Color and intensity of the glow

void main() {
    vec2 uv = gl_FragCoord.xy / resolution;
    vec4 original = texture2D(texture, uv); // Fetch original color
    vec4 glow = vec4(0.0);

    // Convert current fragment to world coordinates
    vec2 worldPos = uv * resolution;

    // Check if the fragment is within the visible camera area
    vec2 halfViewSize = viewSize / 2.0;
    if (worldPos.x < (cameraCenter.x - halfViewSize.x) - glowRadius ||
        worldPos.x > (cameraCenter.x + halfViewSize.x) + glowRadius ||
        worldPos.y < (cameraCenter.y - halfViewSize.y) - glowRadius ||
        worldPos.y > (cameraCenter.y + halfViewSize.y) + glowRadius) {
        gl_FragColor = original; // Skip glow calculation outside the view
        return;
    }

    // Compute glow only for visible fragments
    float radius = glowRadius / resolution.x; // Normalize glow radius
    int samples = 32; // Number of samples for smooth glow

    for (int i = 0; i < samples; ++i) {
        float angle = 2.0 * 3.14159265 * (float(i) / float(samples));
        vec2 offset = vec2(cos(angle), sin(angle)) * radius;
        vec2 sampleUV = uv + offset;

        float dist = length(offset) / radius; // Distance-based falloff
        float falloff = 1.0 - dist;           // Linear falloff
        falloff = max(falloff, 0.0);

        glow += texture2D(texture, sampleUV) * glowColor * falloff;
    }

    glow /= float(samples);  // Average the glow effect
    gl_FragColor = original + glow; // Combine original color with glow
}
)";
};

void checkCollisions(const std::vector<std::shared_ptr<GameObject>> &objects,
                     float dt) {
  const int GRID_SIZE = 200; // Size of each grid cell

  // Hash function to determine grid cell
  auto getGridCell = [](const sf::Vector2f &pos) {
    return sf::Vector2i{static_cast<int>(pos.x) / GRID_SIZE,
                        static_cast<int>(pos.y) / GRID_SIZE};
  };
  struct Vector2iComparator {
    bool operator()(const sf::Vector2i &lhs, const sf::Vector2i &rhs) const {
      if (lhs.x != rhs.x)
        return lhs.x < rhs.x;
      return lhs.y < rhs.y;
    }
  };
  std::map<sf::Vector2i, std::vector<std::shared_ptr<GameObject>>,
           Vector2iComparator>
      grid;

  // Place objects into grid cells
  for (auto &obj : objects) {
    sf::Vector2i cell = getGridCell(obj->getSprite().getPosition());
    grid[cell].push_back(obj);
  }

  // Check collisions within each cell and neighboring cells
  for (auto &[cell, cellObjects] : grid) {
    // Check within current cell
    for (size_t i = 0; i < cellObjects.size(); ++i) {
      for (size_t j = i + 1; j < cellObjects.size(); ++j) {
        cellObjects[i]->onColision(cellObjects[j], dt);
      }
    }

    // Check neighboring cells
    for (int dx = -1; dx <= 1; ++dx) {
      for (int dy = -1; dy <= 1; ++dy) {
        sf::Vector2i neighbor = cell + sf::Vector2i(dx, dy);
        if (grid.count(neighbor)) {
          for (auto &obj1 : cellObjects) {
            for (auto &obj2 : grid[neighbor]) {
              if (obj1 != obj2) {
                obj1->onColision(obj2, dt);
              }
            }
          }
        }
      }
    }
  }
}
class Asteroids : public Drawable, public Tickable {
public:
  Asteroids(std::weak_ptr<Movable> target, int maxAsteroids)
      : target(target), maxAsteroids(10 + maxAsteroids), maxDistance(1000) {}
  virtual void draw(sf::RenderWindow &rw) override {
    for (auto &asteroid : asteroids)
      asteroid->draw(rw);
  }
  virtual void tick(float df) override {
    asteroids.erase(
        std::remove_if(asteroids.begin(), asteroids.end(),
                       [&](const std::shared_ptr<Asteroid> &asteroid) {
                         return PointLen(target.lock()->getPos(),
                                         asteroid->getPos()) > maxDistance;
                       }),
        asteroids.end());
    if (asteroids.size() < maxAsteroids)
      CreateAsteroids();
    for (auto &asteroid : asteroids)
      asteroid->tick(df);
  }
  const std::vector<std::shared_ptr<Asteroid>> &getAsteroids() const {
    return asteroids;
  }

protected:
  void CreateAsteroids() {
    int toCreate = maxAsteroids - asteroids.size();
    auto targetPos = target.lock()->getPos();
    while (--toCreate) {
      auto asteroid = std::make_shared<Asteroid>();

      // Random initial position for the asteroid
      sf::Vector2f initialPos = {
          static_cast<float>(rand() % (2 * maxDistance)) - maxDistance,
          static_cast<float>(rand() % (2 * maxDistance)) - maxDistance};

      // Ensure the asteroid is placed outside a minimum radius from the target
      while (PointLen(initialPos, targetPos) < 450.0f) {
        initialPos = {
            static_cast<float>(rand() % (2 * maxDistance)) - maxDistance,
            static_cast<float>(rand() % (2 * maxDistance)) - maxDistance};
      }

      asteroid->setPos(initialPos.x, initialPos.y);

      // Calculate direction vector towards the target
      sf::Vector2f direction = targetPos - initialPos;

      // Add randomness to the direction
      direction.x +=
          static_cast<float>(rand() % 200 - 100) * 0.1f; // Small random offset
      direction.y += static_cast<float>(rand() % 200 - 100) * 0.1f;

      // Normalize and scale the direction vector to set velocity
      normalize(direction);
      float speed = 50.0f + static_cast<float>(
                                rand() % 50); // Random speed between 50 and 100
      sf::Vector2f velocity = direction * speed;

      // Set the asteroid's velocity (via acceleration for simplicity)
      asteroid->addAcc(velocity);

      asteroids.push_back(asteroid);
    }
  }

private:
  int maxAsteroids = 10;
  int maxDistance = 1000;
  std::vector<std::shared_ptr<Asteroid>> asteroids;
  std::weak_ptr<Movable> target;
};

std::tuple<bool, bool> StartLevel(sf::RenderWindow &window, int level) {
  std::cout << "Hello from the stars" << std::endl;
  const uint StarsSize = 32768;
  fmt::println("Placing the Spaceship");
  float minDistanceFromPlayer = 512.0f + level * 100.0f;
  std::shared_ptr<Spaceship> spaceship =
      std::make_shared<Spaceship>(minDistanceFromPlayer);

  WINDOW_WIDTH = window.getSize().x;
  WINDOW_HEIGHT = window.getSize().y;
  VP_WIDTH = WINDOW_WIDTH / 4.f;
  VP_HEIGHT = VP_HEIGHT / 4.0f;

  fmt::println("Drawing the stars on the sky");
  auto bg = std::make_shared<Background>(
      1e6, sf::Vector2i{StarsSize, StarsSize},
      sf::Vector2i{StarsSize / 2, StarsSize / 2}, window);
  std::vector<std::shared_ptr<Drawable>> drawable = {bg, spaceship};
  std::vector<std::shared_ptr<Tickable>> tickable = {spaceship};
  std::vector<std::shared_ptr<GameObject>> colisable = {spaceship};
  std::shared_ptr<Player> player = std::make_shared<Player>(
      std::filesystem::path("./assets/astronaut.png"),
      sf::Vector2f{0, 0});
  std::shared_ptr<Asteroids> asteroids =
      std::make_shared<Asteroids>(player, level);
  drawable.push_back(asteroids);
  tickable.push_back(asteroids);
  player->setTarget(spaceship);
  drawable.push_back(player);
  tickable.push_back(player);
  colisable.push_back(player);
  inputs input;
  sf::Clock deltaClock, fpsClock;
  int frameCount = 0;
  float fps = 0.0f;
  sf::View view(sf::FloatRect(0.0f, 0.0f, VP_WIDTH, VP_HEIGHT));
  window.setView(view);
  while (window.isOpen() && !player->isWon() && !player->isDead()) {
    sf::Event event;
    while (window.pollEvent(event)) {
      switch (event.type) {
      case sf::Event::Closed:
        window.close();
        break;
      case sf::Event::KeyPressed:
        mapByKeyCode(event, true, input);
        break;
      case sf::Event::KeyReleased:
        mapByKeyCode(event, false, input);
        break;
      default:
        break;
      }
    }
    window.clear(sf::Color::Black);
    for (auto toDraw : drawable)
      toDraw->draw(window);
    view.setCenter(player->getPos());
    WINDOW_WIDTH = window.getSize().x;
    WINDOW_HEIGHT = window.getSize().y;
    VP_WIDTH = WINDOW_WIDTH / 2.f;
    VP_HEIGHT = WINDOW_HEIGHT / 2.f;
    view.setSize({VP_WIDTH, VP_HEIGHT});
    window.setView(view);
    window.display();
    sf::Time dt = deltaClock.restart();
    for (auto tick : tickable)
      tick->tick(dt.asSeconds(), input);

    frameCount++;
    if (fpsClock.getElapsedTime().asSeconds() >= 1.0f) {
      fps = frameCount / fpsClock.getElapsedTime().asSeconds();
      std::cout << "FPS: " << fps << std::endl;

      // Reset for the next second
      frameCount = 0;
      fpsClock.restart();
    }
    colisable.clear();
    colisable.push_back(player);
    colisable.push_back(spaceship);
    for (auto &asteroid : asteroids->getAsteroids())
      colisable.push_back(asteroid);
    checkCollisions(colisable, dt.asSeconds());
  }
  if (player->isWon())
    player->updatePlayerGlobalScore();
  else
    PlayerGameScore = 0;
  return {player->isWon(), player->isDead()};
}

int main() {
  using sc = std::chrono::system_clock;
  using tp = std::chrono::time_point<sc>;
  using namespace std::chrono_literals;
  srand(time(NULL));
  sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT),
                          "Among The Stars");
  int level = 0;
  while (window.isOpen()) {
    auto [isWon, isDead] = StartLevel(window, level);
    sf::View view(sf::FloatRect(0.0f, 0.0f, VP_WIDTH, VP_HEIGHT));
    tp timer_start = sc::now();
    Text txt(std::make_shared<std::function<std::string()>>(
        [&level, &isWon, &isDead]() {
          if (isWon)
            return fmt::format("Level {}, Completed!", level);
          if (isDead)
            return fmt::format("Level {}, Failed!", level);
          return fmt::format("WTF just happened");
        }));
    {
      auto &text = txt.getUnderlayingType();
      auto textRect = text.getLocalBounds();
      text.setOrigin(textRect.left + textRect.width / 2.0f,
                     textRect.top + textRect.height / 2.0f);
    }
    sf::Event ev;
    if (isWon) {
      level++;
      while (window.isOpen() &&
             (sc::now() - timer_start) < 5s) { // Fix condition
        while (window.pollEvent(ev))
          ;
        window.clear(sf::Color::Black);
        window.setView(view);

        // Calculate elapsed time
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            sc::now() - timer_start);
        float seconds = elapsed.count() / 1000.0f;

        // Compute alpha value
        int alpha = 255;
        float fadeTime = seconds - 2.5f;
        float cycle = 2.5f; // Total fade in/out duration
        float progress = fmod(fadeTime, cycle) / cycle;
        alpha =
            static_cast<int>(255 * (1.0f - std::abs(1.0f - 2.0f * progress)));

        // Ensure alpha is within bounds
        alpha = std::clamp(alpha, 0, 255);
        txt.getUnderlayingType().setFillColor(sf::Color(255, 255, 255, alpha));
        txt.setPos(view.getCenter().x - 100,
                   view.getCenter().y - 50); // Center text
        txt.tick(0);
        txt.draw(window);

        window.display();
      }
    }

    if (isDead) {
      Text shouldContinue(std::make_shared<std::function<std::string()>>(
          []() -> std::string { return "Continue? [Y/N]"; }));
      auto &text = shouldContinue.getUnderlayingType();
      auto textRect = text.getLocalBounds();
      text.setOrigin(textRect.left + textRect.width / 2.0f,
                     textRect.top + textRect.height / 2.0f);
      bool isContinuing = false;
      while (!isContinuing && window.isOpen()) { // Fix condition
        while (window.pollEvent(ev)) {
          if (ev.type == sf::Event::KeyPressed) {
            if (ev.key.code == sf::Keyboard::Y)
              isContinuing = true;
            else if (ev.key.code == sf::Keyboard::N)
              return 0;
          }
        }
        window.clear(sf::Color::Black);
        window.setView(view);

        // Calculate elapsed time
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            sc::now() - timer_start);
        float seconds = elapsed.count() / 1000.0f;

        // Compute alpha value
        int alpha = 255;
        float fadeTime = seconds - 2.5f;
        float cycle = 2.5f; // Total fade in/out duration
        float progress = fmod(fadeTime, cycle) / cycle;
        alpha =
            static_cast<int>(255 * (1.0f - std::abs(1.0f - 2.0f * progress)));

        // Ensure alpha is within bounds
        alpha = std::clamp(alpha, 0, 255);
        txt.getUnderlayingType().setFillColor(sf::Color(255, 255, 255, alpha));
        shouldContinue.getUnderlayingType().setFillColor(
            sf::Color(255, 255, 255, alpha));
        txt.setPos(view.getCenter().x - 100,
                   view.getCenter().y - 50); // Center text
        shouldContinue.setPos(view.getCenter().x - 100,
                              view.getCenter().y + 150);
        shouldContinue.tick(0);
        txt.tick(0);
        shouldContinue.draw(window);
        txt.draw(window);

        window.display();
      }
      level = 0;
    }
  }
}
