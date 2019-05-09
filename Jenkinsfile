pipeline {
  agent {
    node {
      label 'ibmi'
    }
  }
  stages {
    stage('configure') {
      environment {
        OBJECT_MODE = '64'
        CC = 'gcc'
        CXX = 'g++'
        ac_cv_func_clock_gettime = "no"
        ac_cv_func_getpeereid = "no"
      }
      steps {
        sh 'cp /QOpenSys/jenkins/openssh.cache config.cache || :'
        sh 'autoreconf'
        sh '''./configure \
            blibpath=/QOpenSys/pkgs/lib:/QOpenSys/usr/lib \
            --config-cache \
            --with-cflags="-maix64 -std=c99" \
            --with-cppflags="-I/QOpenSys/pkgs/include" \
            --with-ldflags="-maix64 -Wl,-brtl -L/QOpenSys/pkgs/lib" \
            --sysconfdir=/QOpenSys/etc/ssh \
            --with-pid-dir=/tmp \
            --with-ssl-dir=/QOpenSys/etc/ssl \
            --with-sandbox=rlimit \
            --with-privsep-user=qsshd \
            --with-privsep-path=/QOpenSys/var/empty \
            --with-default-path=/QOpenSys/usr/bin:/usr/ccs/bin:/QOpenSys/usr/bin/X11:/usr/sbin:/usr/bin:/QOpenSys/pkgs/bin \
            --without-pam \
            --without-rpath \
            --disable-utmp \
            --disable-utmpx \
            --disable-wtmp \
            --disable-wtmpx \
            --disable-libutil
        '''
      }
    }
    stage('build') {
      environment {
        OBJECT_MODE = '64'
      }

      steps {
        sh 'make -j4 all'
      }
    }
    stage('test') {
      environment {
        OBJECT_MODE = '64'
      }

      steps {
        sh 'make tests'
      }
    }
  }
}
