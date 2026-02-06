module;
#include <wobjectimpl.h>
module Artifact.Test.RenderQueue;

import std;
import Artifact.Render.Queue.Service;

namespace Artifact
{
 class ArtifactTestRenderQueue::Impl
 {
 private:
  bool jobAdded;
  bool jobStatusChanged;
  int jobCount;
  int jobIndex;
  ArtifactRenderJob::Status jobStatus;

 public:
  Impl();
  ~Impl();
  void setupCallbacks();
  void testAddRenderQueue();
  void testRemoveRenderQueue();
  void testRemoveAllRenderQueues();
  void testStartAllJobs();
  void testPauseAllJobs();
  void testCancelAllJobs();
  void testJobCount();
  void testGetJob();
  void testGetTotalProgress();
  void testJobStatusChanged();
 };

 ArtifactTestRenderQueue::Impl::Impl()
  : jobAdded(false)
  , jobStatusChanged(false)
  , jobCount(0)
  , jobIndex(-1)
  , jobStatus(ArtifactRenderJob::Status::Pending)
 {
  setupCallbacks();
 }

 ArtifactTestRenderQueue::Impl::~Impl()
 {
 }

 void ArtifactTestRenderQueue::Impl::setupCallbacks()
 {
  auto& renderQueueService = ArtifactRenderQueueService::getInstance();

  renderQueueService.setJobAddedCallback([this](int index) {
   jobAdded = true;
   jobCount++;
   jobIndex = index;
  });

  renderQueueService.setJobStatusChangedCallback([this](int index, ArtifactRenderJob::Status status) {
   jobStatusChanged = true;
   jobIndex = index;
   jobStatus = status;
  });

  renderQueueService.setJobRemovedCallback([this](int index) {
   jobCount--;
  });
 }

 void ArtifactTestRenderQueue::Impl::testAddRenderQueue()
 {
  auto& renderQueueService = ArtifactRenderQueueService::getInstance();
  int initialCount = renderQueueService.jobCount();

  renderQueueService.addRenderQueue();

  if (renderQueueService.jobCount() == initialCount + 1) {
   qDebug() << "testAddRenderQueue passed: Job count increased by 1";
  } else {
   qDebug() << "testAddRenderQueue failed: Job count did not increase";
  }
 }

 void ArtifactTestRenderQueue::Impl::testRemoveRenderQueue()
 {
  auto& renderQueueService = ArtifactRenderQueueService::getInstance();
  
  // テストのためにジョブを追加
  renderQueueService.addRenderQueue();
  int initialCount = renderQueueService.jobCount();

  // このメソッドはまだ実装されていないので、テストはスキップ
  // renderQueueService.removeRenderQueue();
  qDebug() << "testRemoveRenderQueue: Method not implemented";
 }

 void ArtifactTestRenderQueue::Impl::testRemoveAllRenderQueues()
 {
  auto& renderQueueService = ArtifactRenderQueueService::getInstance();
  
  // テストのためにジョブを追加
  for (int i = 0; i < 3; ++i) {
   renderQueueService.addRenderQueue();
  }

  renderQueueService.removeAllRenderQueues();

  if (renderQueueService.jobCount() == 0) {
   qDebug() << "testRemoveAllRenderQueues passed: Queue is empty";
  } else {
   qDebug() << "testRemoveAllRenderQueues failed: Queue is not empty";
  }
 }

 void ArtifactTestRenderQueue::Impl::testStartAllJobs()
 {
  auto& renderQueueService = ArtifactRenderQueueService::getInstance();
  
  // テストのためにジョブを追加
  renderQueueService.addRenderQueue();

  renderQueueService.startAllJobs();

  ArtifactRenderJob job = renderQueueService.getJob(0);
  if (job.status == ArtifactRenderJob::Status::Rendering) {
   qDebug() << "testStartAllJobs passed: Job status changed to rendering";
  } else {
   qDebug() << "testStartAllJobs failed: Job status did not change to rendering";
  }
 }

 void ArtifactTestRenderQueue::Impl::testPauseAllJobs()
 {
  auto& renderQueueService = ArtifactRenderQueueService::getInstance();
  
  // テストのためにジョブを追加して開始
  renderQueueService.addRenderQueue();
  renderQueueService.startAllJobs();

  renderQueueService.pauseAllJobs();

  ArtifactRenderJob job = renderQueueService.getJob(0);
  if (job.status == ArtifactRenderJob::Status::Pending) {
   qDebug() << "testPauseAllJobs passed: Job status changed to pending";
  } else {
   qDebug() << "testPauseAllJobs failed: Job status did not change to pending";
  }
 }

 void ArtifactTestRenderQueue::Impl::testCancelAllJobs()
 {
  auto& renderQueueService = ArtifactRenderQueueService::getInstance();
  
  // テストのためにジョブを追加して開始
  renderQueueService.addRenderQueue();
  renderQueueService.startAllJobs();

  renderQueueService.cancelAllJobs();

  ArtifactRenderJob job = renderQueueService.getJob(0);
  if (job.status == ArtifactRenderJob::Status::Canceled) {
   qDebug() << "testCancelAllJobs passed: Job status changed to canceled";
  } else {
   qDebug() << "testCancelAllJobs failed: Job status did not change to canceled";
  }
 }

 void ArtifactTestRenderQueue::Impl::testJobCount()
 {
  auto& renderQueueService = ArtifactRenderQueueService::getInstance();
  int initialCount = renderQueueService.jobCount();

  renderQueueService.addRenderQueue();
  renderQueueService.addRenderQueue();

  if (renderQueueService.jobCount() == initialCount + 2) {
   qDebug() << "testJobCount passed: Job count is correct";
  } else {
   qDebug() << "testJobCount failed: Job count is incorrect";
  }
 }

 void ArtifactTestRenderQueue::Impl::testGetJob()
 {
  auto& renderQueueService = ArtifactRenderQueueService::getInstance();
  
  // テストのためにジョブを追加
  renderQueueService.addRenderQueue();

  if (renderQueueService.jobCount() > 0) {
   ArtifactRenderJob job = renderQueueService.getJob(0);
   if (!job.compositionName.isEmpty()) {
    qDebug() << "testGetJob passed: Job retrieved successfully";
   } else {
    qDebug() << "testGetJob failed: Job composition name is empty";
   }
  } else {
   qDebug() << "testGetJob failed: No jobs in queue";
  }
 }

 void ArtifactTestRenderQueue::Impl::testGetTotalProgress()
 {
  auto& renderQueueService = ArtifactRenderQueueService::getInstance();
  
  // テストのためにジョブを追加
  renderQueueService.addRenderQueue();

  int progress = renderQueueService.getTotalProgress();
  if (progress >= 0 && progress <= 100) {
   qDebug() << "testGetTotalProgress passed: Progress is in valid range";
  } else {
   qDebug() << "testGetTotalProgress failed: Progress is out of range";
  }
 }

 void ArtifactTestRenderQueue::Impl::testJobStatusChanged()
 {
  auto& renderQueueService = ArtifactRenderQueueService::getInstance();
  
  // テストのためにジョブを追加
  renderQueueService.addRenderQueue();
  jobStatusChanged = false;

  renderQueueService.startAllJobs();

  if (jobStatusChanged) {
   qDebug() << "testJobStatusChanged passed: Status changed callback was called";
  } else {
   qDebug() << "testJobStatusChanged failed: Status changed callback was not called";
  }
 }

 ArtifactTestRenderQueue::ArtifactTestRenderQueue():impl_(new Impl)
 {
 }

 ArtifactTestRenderQueue::~ArtifactTestRenderQueue()
 {
  delete impl_;
 }

 void ArtifactTestRenderQueue::runAllTests()
 {
  qDebug() << "Running Render Queue Tests...";

  // クリアしてからテストを開始
  auto& renderQueueService = ArtifactRenderQueueService::getInstance();
  renderQueueService.removeAllRenderQueues();

  // すべてのテストを実行
  impl_->testAddRenderQueue();
  impl_->testJobCount();
  impl_->testGetJob();
  impl_->testGetTotalProgress();
  impl_->testStartAllJobs();
  impl_->testJobStatusChanged();
  impl_->testPauseAllJobs();
  impl_->testCancelAllJobs();
  impl_->testRemoveAllRenderQueues();

  qDebug() << "Render Queue Tests Completed!";
 }
};