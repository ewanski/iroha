#!/usr/bin/env groovy

def doDebugBuild() {
	if (params.ARMv7) {
		PARALLELISM = 1;
	}
	sh "docker network create ${env.IROHA_NETWORK}"

	docker.image('postgres:9.5').run(""
	    + " -e POSTGRES_USER=${env.IROHA_POSTGRES_USER}"
	    + " -e POSTGRES_PASSWORD=${env.IROHA_POSTGRES_PASSWORD}"
	    + " --name ${env.IROHA_POSTGRES_HOST}"
	    + " --network=${env.IROHA_NETWORK}")

	docker.image('redis:3.2.8').run(""
	    + " --name ${env.IROHA_REDIS_HOST}"
	    + " --network=${env.IROHA_NETWORK}")

	def platform = sh(script: 'uname -m', returnStdout: true).trim()
	// TODO: replace Github pull path as soon as multiplatform support will be merged
	sh "curl -L -o /tmp/${env.GIT_COMMIT}/Dockerfile --create-dirs https://raw.githubusercontent.com/hyperledger/iroha/${env.GIT_COMMIT}/docker/develop/${platform}/Dockerfile"
	// pull docker image in case we don't have one
	// speeds up consequent image builds as we simply tag them 
	sh "docker pull ${DOCKER_BASE_IMAGE_DEVELOP}"
	if (env.BRANCH_NAME == 'develop') {
	    iC = docker.build("hyperledger/iroha:${GIT_COMMIT}-${BUILD_NUMBER}", "-f /tmp/${env.GIT_COMMIT}/Dockerfile /tmp/${env.GIT_COMMIT} --build-arg PARALLELISM=${PARALLELISM}")
		docker.withRegistry('https://registry.hub.docker.com', 'docker-hub-credentials') {
        	iC.push("${platform}-develop")
        }
	}
	else {
	    iC = docker.build("hyperledger/iroha-workflow:${GIT_COMMIT}-${BUILD_NUMBER}", "-f /tmp/${env.GIT_COMMIT}/Dockerfile /tmp/${env.GIT_COMMIT} --build-arg PARALLELISM=${PARALLELISM}")
	}
	sh "rm -rf /tmp/${env.GIT_COMMIT}"
	iC.inside(""
	    + " -e IROHA_POSTGRES_HOST=${env.IROHA_POSTGRES_HOST}"
	    + " -e IROHA_POSTGRES_PORT=${env.IROHA_POSTGRES_PORT}"
	    + " -e IROHA_POSTGRES_USER=${env.IROHA_POSTGRES_USER}"
	    + " -e IROHA_POSTGRES_PASSWORD=${env.IROHA_POSTGRES_PASSWORD}"
	    + " -e IROHA_REDIS_HOST=${env.IROHA_REDIS_HOST}"
	    + " -e IROHA_REDIS_PORT=${env.IROHA_REDIS_PORT}"
	    + " --network=${env.IROHA_NETWORK}"
	    + " -v /var/jenkins/ccache:${CCACHE_DIR}") {

	    def scmVars = checkout scm
	    env.IROHA_VERSION = "0x${scmVars.GIT_COMMIT}"
	    env.IROHA_HOME = "/opt/iroha"
	    env.IROHA_BUILD = "${env.IROHA_HOME}/build"
	    env.IROHA_RELEASE = "${env.IROHA_HOME}/docker/release"

	    sh """
	        ccache --version
	        ccache --show-stats
	        ccache --zero-stats
	        ccache --max-size=5G
	    """	
	    sh """
	        cmake \
	          -DCOVERAGE=ON \
	          -DTESTING=ON \
	          -H. \
	          -Bbuild \
	          -DCMAKE_BUILD_TYPE=${params.BUILD_TYPE} \
	          -DIROHA_VERSION=${env.IROHA_VERSION}
	    """
	    sh "cmake --build build -- -j${params.PARALLELISM}"
	    sh "ccache --show-stats"
	    // sh "cmake --build build --target test"
	    // sh "cmake --build build --target cppcheck"	    
	    
	    // Sonar
	    if (env.CHANGE_ID != null) {
	        sh """
	            sonar-scanner \
	                -Dsonar.github.disableInlineComments \
	                -Dsonar.github.repository='hyperledger/iroha' \
	                -Dsonar.analysis.mode=preview \
	                -Dsonar.login=${SONAR_TOKEN} \
	                -Dsonar.projectVersion=${BUILD_TAG} \
	                -Dsonar.github.oauth=${SORABOT_TOKEN} \
	                -Dsonar.github.pullRequest=${CHANGE_ID}
	        """
	    }

	    // TODO: replace with upload to artifactory server
        // only develop branch
        if ( env.BRANCH_NAME == "develop" ) {
            //archive(includes: 'build/bin/,compile_commands.json')
        }
        if ( env.LCOV_ALREADY_BUILT == 0 || ! params.MacOS ) {
        	env.LCOV_ALREADY_BUILT = 1
        	sh "lcov --capture --directory build --config-file .lcovrc --output-file build/reports/coverage_full.info"
		    sh "lcov --remove build/reports/coverage_full.info '/usr/*' 'schema/*' --config-file .lcovrc -o build/reports/coverage_full_filtered.info"
		    sh "python /tmp/lcov_cobertura.py build/reports/coverage_full_filtered.info -o build/reports/coverage.xml"                                
		    cobertura autoUpdateHealth: false, autoUpdateStability: false, coberturaReportFile: '**/build/reports/coverage.xml', conditionalCoverageTargets: '75, 50, 0', failUnhealthy: false, failUnstable: false, lineCoverageTargets: '75, 50, 0', maxNumberOfBuilds: 50, methodCoverageTargets: '75, 50, 0', onlyStable: false, zoomCoverageChart: false
        }	    
	}
}
return this
