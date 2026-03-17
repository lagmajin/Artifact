module;

//#include "WickedEngine.h"
#include <wobjectdefs.h>

#include <memory>
#include <QWidget>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module ArtifactRenderManagerWidget;





namespace Artifact {

class RendererQueueModel
{
	
};


 class ArtifactRenderManagerWidgetPrivate;

 //class wi::Application;

   export class ArtifactRenderManagerWidget :public QWidget{
	//Q_OBJECT
   private:
	//std::unique_ptr<ArtifactRenderManagerWidgetPrivate> pImpl_;
	bool initialized = false;
	//wi::Application* app=nullptr;
   protected:
	bool event(QEvent* e) override;
   public:
	explicit ArtifactRenderManagerWidget(QWidget* parent = nullptr);
	~ArtifactRenderManagerWidget();
	void clear();
   };

 }  // namespace Artifact
