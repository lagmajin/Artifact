#ifndef RENDERQUEUETEST_H
#define RENDERQUEUETEST_H

#include <QObject>
#include <QTest>

class RenderQueueTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
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

#endif // RENDERQUEUETEST_H