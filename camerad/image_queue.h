/**
 * @file     image_queue.h
 * @brief    implements a thread-safe blocking queue
 * @author   David Hale <dhale@astro.caltech.edu>
 *
 */

#pragma once

namespace Camera {

  /***** Camera::ImageQueue ***************************************************/
  /**
   * @brief    template-based, thread-safe FIFO queue
   * @details  This wraps the queue push and pop with mutexes and condition
   *           variables.
   *
   */
  template <typename T>
  class ImageQueue {

    private:
      std::queue<std::shared_ptr<T>> q;
      std::mutex m;
      std::condition_variable cv;

    public:
      /***** Camera::ImageQueue::enqueue ******************************************/
      /**
       * @brief      adds an image to the end of the queue
       * @details    Ownership of the image is transferred to the queue.
       * @param[in]  image  shared_ptr to the <T> typed image buffer
       *
       */
      void enqueue(std::shared_ptr<T> image) {
        {
        std::lock_guard<std::mutex> lock(m);
        q.push(std::move(image));
        }
        cv.notify_one();
      }

      /***** Camera::ImageQueue::dequeue ******************************************/
      /**
       * @brief      removes and returns next image from the queue
       * @details    This blocks while the queue is empty.
       */
      std::shared_ptr<T> dequeue() {
        std::unique_lock<std::mutex> lock(m);
        cv.wait(lock, [&]{ return !q.empty(); });

        auto image = std::move( q.front() );
        q.pop();
        return image;
      }
  };
  /***** Camera::ImageQueue ***************************************************/
}
