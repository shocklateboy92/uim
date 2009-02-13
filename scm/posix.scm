;;; posix.scm: posix functions for uim.
;;;
;;; Copyright (c) 2009-2009 uim Project http://code.google.com/p/uim/
;;;
;;; All rights reserved.
;;;
;;; Redistribution and use in source and binary forms, with or without
;;; modification, are permitted provided that the following conditions
;;; are met:
;;; 1. Redistributions of source code must retain the above copyright
;;;    notice, this list of conditions and the following disclaimer.
;;; 2. Redistributions in binary form must reproduce the above copyright
;;;    notice, this list of conditions and the following disclaimer in the
;;;    documentation and/or other materials provided with the distribution.
;;; 3. Neither the name of authors nor the names of its contributors
;;;    may be used to endorse or promote products derived from this software
;;;    without specific prior written permission.
;;;
;;; THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND
;;; ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
;;; IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
;;; ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
;;; FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
;;; DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
;;; OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
;;; HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
;;; LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
;;; OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
;;; SUCH DAMAGE.
;;;;

(define (process-execute file . args)
  (let-optionals* args ((argv (list file))
                        (envp #f))
    (if envp
        (execve file argv envp)
        (execvp file argv))))

(define (process-io file . args)
  (let-optionals* args ((argv (list file)))
    (and-let* ((pin (create-pipe))
               (pout (create-pipe))
               (pin-in  (car pin))
               (pin-out (cdr pin))
               (pout-in  (car pout))
               (pout-out (cdr pout)))
      (let ((pid (process-fork)))
        (cond ((< pid 0)
               (begin
                 (uim-notify-fatal "cannot fork")
                 (file-close pin-in)
                 (file-close pin-out)
                 (file-close pout-in)
                 (file-close pout-out)
                 #f))
              ((= 0 pid) ;; child
               (setsid)
               (file-close pin-out)
               (if (< (duplicate-fileno pin-in 0) 0)
                   (uim-notify-fatal "cannot duplicate stdin"))
               (file-close pin-in)

               (file-close pout-in)
               (if (< (duplicate-fileno pout-out 1) 0)
                   (uim-notify-fatal "cannot duplicate stdout"))
               (file-close pout-out)

               (process-execute file argv)
               (_exit 1)
               )
              (else ;; parent
               (file-close pin-in)
               (file-close pout-out)
               (cons pout-in pin-out)))))))

(define (process-with-daemon file . args)
  (let-optionals* args ((argv (list file)))
    (let ((pid (process-fork)))
      (cond ((< pid 0)
             (begin
               (uim-notify-fatal "cannot fork")
               #f))
            ((= 0 pid) ;; child
             (daemon 0 1)
             (process-execute file argv)
             (_exit 1))
            (else
             pid)))))
