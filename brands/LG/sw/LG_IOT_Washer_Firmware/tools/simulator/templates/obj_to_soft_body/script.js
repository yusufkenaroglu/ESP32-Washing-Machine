
 
import * as THREE from "https://unpkg.com/three@0.126.1/build/three.module.js";
import { OrbitControls } from "https://unpkg.com/three@0.125.2/examples/jsm/controls/OrbitControls.js";
import { Curves } from '//cdn.skypack.dev/three@0.132/examples/jsm/curves/CurveExtras?min'
import { Geometry } from 'https://unpkg.com/three@0.125.2/examples/jsm/deprecated/Geometry.js'

import { PointerLockControls } from "https://cdn.skypack.dev/three@0.132.0/examples/jsm/controls/PointerLockControls";
import { GLTFLoader } from "https://cdn.skypack.dev/three@0.132.0/examples/jsm/loaders/GLTFLoader";
import { ConvexObjectBreaker } from "https://cdn.skypack.dev/three@0.132.0/examples/jsm/misc/ConvexObjectBreaker";
import { ConvexGeometry } from "https://cdn.skypack.dev/three@0.132.0/examples/jsm/geometries/ConvexGeometry";
import * as BufferGeometryUtils from 'https://cdn.skypack.dev/three@0.132.0/examples/jsm/utils/BufferGeometryUtils';

import { GUI } from "https://unpkg.com/three@0.125.2/examples/jsm/libs/dat.gui.module.js";
import Stats from "https://unpkg.com/three@0.141.0/examples/jsm/libs/stats.module.js";

import SimplexNoise from "https://cdn.skypack.dev/simplex-noise@2.4.0";
import { SimplifyModifier } from 'https://cdn.skypack.dev/three@0.136/examples/jsm/modifiers/SimplifyModifier.js';
import { RGBELoader } from 'https://cdn.skypack.dev/three@0.125.2/examples/jsm/loaders/RGBELoader.js';

	let keyStates = {};
  let box;
  
  

// Graphics variables
			let container, stats;
			let camera, controls, scene, renderer;
			const clock = new THREE.Clock();
			let clickRequest = false;
			const mouseCoords = new THREE.Vector2();
			const raycaster = new THREE.Raycaster();
			const ballMaterial = new THREE.MeshPhongMaterial( { color: 0x202020 } );
			const pos = new THREE.Vector3();
			const quat = new THREE.Quaternion();

			// Physics variables
			const gravityConstant = - 9.8;
			let physicsWorld;
			const rigidBodies = [];
			const softBodies = [];
			const margin = 0.05;
			let transformAux1;
			let softBodyHelpers;


let loader = new GLTFLoader();
let mixer;
let mesh;

let textureLoader = new THREE.TextureLoader();



			Ammo().then( function ( AmmoLib ) {

				Ammo = AmmoLib;

				init();
				animate();

			} );

			function init() {

				initGraphics();
				initPhysics();
				createObjects();
				initInput();

			}

			function initGraphics() {

        container = document.createElement( 'div' );
				document.body.appendChild( container );

				camera = new THREE.PerspectiveCamera( 60, window.innerWidth / window.innerHeight, 0.2, 2000 );

				scene = new THREE.Scene();
				scene.background = new THREE.Color( 0xbfd1e5 );

				camera.position.set( - 7, 5, 8 );

				renderer = new THREE.WebGLRenderer();
				renderer.setPixelRatio( window.devicePixelRatio );
				renderer.setSize( window.innerWidth, window.innerHeight );
				renderer.shadowMap.enabled = true;
				container.appendChild( renderer.domElement );

				controls = new OrbitControls( camera, renderer.domElement );
				controls.target.set( 0, 2, 0 );
				controls.update();

				const ambientLight = new THREE.AmbientLight( 0x404040 );
				scene.add( ambientLight );

				const light = new THREE.DirectionalLight( 0xffffff, 1 );
				light.position.set( - 10, 10, 5 );
				light.castShadow = true;
				const d = 20;
				light.shadow.camera.left = - d;
				light.shadow.camera.right = d;
				light.shadow.camera.top = d;
				light.shadow.camera.bottom = - d;
				light.shadow.camera.near = 2;
				light.shadow.camera.far = 50;
				light.shadow.mapSize.x = 1024;
				light.shadow.mapSize.y = 1024;

				scene.add( light );

				stats = new Stats();
				stats.domElement.style.position = 'absolute';
				stats.domElement.style.top = '0px';
				container.appendChild( stats.domElement );
				window.addEventListener( 'resize', onWindowResize );

			}

			function initPhysics() {
				// Physics configuration
				const collisionConfiguration = new Ammo.btSoftBodyRigidBodyCollisionConfiguration();
				const dispatcher = new Ammo.btCollisionDispatcher( collisionConfiguration );
				const broadphase = new Ammo.btDbvtBroadphase();
				const solver = new Ammo.btSequentialImpulseConstraintSolver();
				const softBodySolver = new Ammo.btDefaultSoftBodySolver();
				physicsWorld = new Ammo.btSoftRigidDynamicsWorld( dispatcher, broadphase, solver, collisionConfiguration, softBodySolver );
				physicsWorld.setGravity( new Ammo.btVector3( 0, gravityConstant, 0 ) );
				physicsWorld.getWorldInfo().set_m_gravity( new Ammo.btVector3( 0, gravityConstant, 0 ) );

				transformAux1 = new Ammo.btTransform();
				softBodyHelpers = new Ammo.btSoftBodyHelpers();
			}


			function createObjects() {

				// Ground
				pos.set( 0, - 0.5, 0 );
				quat.set( 0, 0, 0, 1 );
				const ground = createParalellepiped( 40, 1, 40, 0, pos, quat, new THREE.MeshPhongMaterial( { color: 0xFFFFFF } ) );
				ground.castShadow = true;
				ground.receiveShadow = true;
        
        
				textureLoader.load( 'https://threejs.org/examples/textures/grid.png', function ( texture ) {

					texture.wrapS = THREE.RepeatWrapping;
					texture.wrapT = THREE.RepeatWrapping;
					texture.repeat.set( 40, 40 );
					ground.material.map = texture;
					ground.material.needsUpdate = true;

				} );


				// Ramp
				pos.set( 3, 1, 0 );
				quat.setFromAxisAngle( new THREE.Vector3( 0, 0, 1 ), 30 * Math.PI / 180 );
				const obstacle = createParalellepiped( 10, 1, 4, 0, pos, quat, new THREE.MeshPhongMaterial( { color: 0x606060 } ) );
				obstacle.castShadow = true;
				obstacle.receiveShadow = true;
        
        
 
 
let ratio = 1350 / 900;
let mapDef = textureLoader.load( 'https://threejs.org/examples/textures/uv_grid_opengl.jpg' );
let mat = new THREE.MeshPhongMaterial({ map: mapDef, wireframe: true });
let mat1 = new THREE.MeshPhongMaterial({ map: mapDef, wireframe: false, side: THREE.DoubleSide });
   
let material = new THREE.MeshPhongMaterial( {color: "grey", wireframe: false, side: THREE.DoubleSide, transparent: false} );
let meshMaterial = new THREE.MeshLambertMaterial( {color: 0xffffff, wireframe: false, opacity: 0.5,transparent: true } );
 
 
 
 let a1 = 'https://threejs.org/examples/models/gltf/LeePerrySmith/LeePerrySmith.glb';
 let a2 = 'https://cdn.devdojo.com/assets/3d/parrot.glb';
 
 new GLTFLoader().load( a2, function ( gltf ) {
 var threeObject = gltf.scene.children[0];
      
  for(let i = 0; i<gltf.scene.children.length; i++){
    threeObject = gltf.scene.children[i];			
  }

  let mapmat;
  gltf.scene.traverse( child => {
    if ( child.isMesh ) {
      child.castShadow = true;
      child.receiveShadow = true;
      if ( child.material.map ) {
        mapmat = child.material; // кэшируем материал
        child.material.map.anisotropy = 8; // сглаживание
      }
    }
  });	 





    // берем геометрию 3д модели
    let threeG3d = threeObject.geometry;
  //  threeG3d = BufferGeometryUtils.mergeVertices( threeG3d );           
      
    let pos1Atrib = threeG3d.attributes.position;
    let vec3 = new THREE.Vector3();
    
    for (let i = 0; i < pos1Atrib.count; i++) {
      vec3.fromBufferAttribute(pos1Atrib, i);
      vec3.multiplyScalar( 0.1 ); // масштабируем атрб. точек
      pos1Atrib.setXYZ(i, vec3.x, vec3.y, vec3.z);
    }


    let dod = setupAttributes( threeG3d ); // способ 1 после преобраз геом в буф, с назначением центра 
	//		dod = BufferGeometryUtils.mergeVertices( dod );
  
    dod.computeVertexNormals();
    dod.computeBoundingSphere();
 // 	createSoftVolume( dod, 150, 120 ); // на этом этапе не такая чувствительность коллизий
  
  
  
  
 		/////////////
    
 		let posAtrAr1 = dod.attributes.position.array;
 		let vertices = [];
    // создаем массив с точками способ 2
		for ( let i = 0; i < posAtrAr1.length; i +=3 ) {
			let v1 = new THREE.Vector3(posAtrAr1[i], posAtrAr1[i+1], posAtrAr1[i+2]); // x,y,z
			vertices.push(v1);
		} 

		let normals = dod.attributes.normals;
    
    let uvs = []; // кэшируем оригинальные uv координаты
    uvs = new Float32Array(dod.attributes.uv.array);
		
		let indices = [];
		let posAtr = dod.attributes.position;
    let posAtrAr = dod.attributes.position.array;
    indices = dod.index; // кэшируем оригинальные индексы, чтобы затем из точек и индексов сделать геометрию новую
    

	// создаем точки
    let pointsMaterial = new THREE.PointsMaterial( {color: 0x0080ff,size: 0.1, alphaTest: 0.5} );
    let pointsGeometry = new THREE.BufferGeometry().setFromPoints( vertices ); //буфергеом из точек
    let points = new THREE.Points( pointsGeometry, pointsMaterial );
    scene.add(points);
 
  // создаем меш из точек
    let mesh2 = new THREE.Mesh(pointsGeometry, mat);
    mesh2.material.side = THREE.DoubleSide;
    
    scene.add(mesh2);
		mesh2.geometry.setFromPoints(vertices);  
    mesh2.geometry.setIndex( indices );  
 //   mesh2.geometry.setAttribute( 'position', new THREE.Float32BufferAttribute( tes1, 3) ); 
//		mesh2.geometry.setAttribute( 'normal', new THREE.Float32BufferAttribute( normals, 3 )); 
    mesh2.geometry.setAttribute( 'uv', new THREE.Float32BufferAttribute( uvs, 2 ))
		mesh2.geometry.computeVertexNormals();
    mesh2.geometry.needsUpdate = true;
    mesh2.geometry.computeBoundingSphere();
    mesh2.geometry.translate(0, 10, 0);
    mesh2.material = mat1;
     
    createSoftVolume( mesh2.geometry, 150, 120 ); // после преобраз. через точки лучше коллизии с мягким телом
  

        
        
});
    
    
 	
  

        
   

 
 
 
 
 
 // перевод геометрии в буферную:	
    function setupAttributes( geometry ) {
        const vectors = [
					new THREE.Vector3(), // new THREE.Vector3( 1, 0, 0 ),
					new THREE.Vector3(), // new THREE.Vector3( 0, 1, 0 ),
					new THREE.Vector3() // new THREE.Vector3( 0, 0, 1 )
				];
				const position = geometry.attributes.position;
				const centers = new Float32Array( position.count * 3 );

				for ( let i = 0, l = position.count; i < l; i ++ ) {
					vectors[ i % 3 ].toArray( centers, i * 3 );
				}
				geometry.setAttribute( 'center', new THREE.BufferAttribute( centers, 3 ) );
        return geometry;
			}
        


}






function processGeometry( bufGeometry ) {

  // Obtain a Geometry
  let bfgeometry = new Geometry().fromBufferGeometry(bufGeometry); // если что, он воссозд из точек
  bfgeometry.mergeVertices();

  // Convert again to BufferGeometry, indexed
  var indexedBufferGeom = createIndexedBufferGeometryFromGeometry( bfgeometry );

  // Create index arrays mapping the indexed vertices to bufGeometry vertices
  mapIndices( bufGeometry, indexedBufferGeom );

}




function createIndexedBufferGeometryFromGeometry( geometry ) {

  var numVertices = geometry.vertices.length;
  var numFaces = geometry.faces.length;

  var bufferGeom = new THREE.BufferGeometry();
  var vertices = new Float32Array( numVertices * 3 );
  var indices = new ( numFaces * 3 > 65535 ? Uint32Array : Uint16Array )( numFaces * 3 );

  for ( var i = 0; i < numVertices; i++ ) {
    var p = geometry.vertices[ i ];
    var i3 = i * 3;
    vertices[ i3 ] = p.x;
    vertices[ i3 + 1 ] = p.y;
    vertices[ i3 + 2 ] = p.z;
  }

  for ( var i = 0; i < numFaces; i++ ) {
    var f = geometry.faces[ i ];
    var i3 = i * 3;
    indices[ i3 ] = f.a;
    indices[ i3 + 1 ] = f.b;
    indices[ i3 + 2 ] = f.c;
  }

  bufferGeom.setIndex( new THREE.BufferAttribute( indices, 1 ) );
  bufferGeom.addAttribute( 'position', new THREE.BufferAttribute( vertices, 3 ) );

  return bufferGeom;
}


function isEqual( x1, y1, z1, x2, y2, z2 ) {
  var delta = 0.000001;
  return Math.abs( x2 - x1 ) < delta &&
    Math.abs( y2 - y1 ) < delta &&
    Math.abs( z2 - z1 ) < delta;
}

function mapIndices( bufGeometry, indexedBufferGeom ) {
  
  // ertices indices ndex in bufGeometry
  var vertices = bufGeometry.attributes.position.array;
  var idxVertices = indexedBufferGeom.attributes.position.array;
  var indices = indexedBufferGeom.index.array;

  var numIdxVertices = idxVertices.length / 3;
  var numVertices = vertices.length / 3;

  bufGeometry.ammoVertices = idxVertices;
  bufGeometry.ammoIndices = indices;
  bufGeometry.ammoIndexAssociation = [];

  for ( var i = 0; i < numIdxVertices; i++ ) {

    var association = [];
    bufGeometry.ammoIndexAssociation.push( association );

    var i3 = i * 3;
    for ( var j = 0; j < numVertices; j++ ) {
      var j3 = j * 3;
      if ( isEqual( idxVertices[ i3 ], idxVertices[ i3 + 1 ],  idxVertices[ i3 + 2 ],
                   vertices[ j3 ], vertices[ j3 + 1 ], vertices[ j3 + 2 ] ) ) {
        association.push( j3 );
      }
    }
  }
}




function createSoftVolume( bufferGeom, mass, pressure ) {

  processGeometry( bufferGeom );
  
let mapDef2 = textureLoader.load( 'https://threejs.org/examples/textures/uv_grid_opengl.jpg' );
let mat2 = new THREE.MeshPhongMaterial({ map: mapDef2, wireframe: false, side: THREE.DoubleSide });

  var volume = new THREE.Mesh( bufferGeom, mat2 );
  volume.castShadow = true;
  volume.receiveShadow = true;
  volume.frustumCulled = false;
  scene.add( volume );

  textureLoader.load( "textures/colors.png", function( texture ) {
    volume.material.map = texture;
    volume.material.needsUpdate = true;
  } );



  // Volume physic object
  var volumeSoftBody = softBodyHelpers.CreateFromTriMesh(
    physicsWorld.getWorldInfo(),
    bufferGeom.ammoVertices,
    bufferGeom.ammoIndices,
    bufferGeom.ammoIndices.length / 3,
    true );

  var sbConfig = volumeSoftBody.get_m_cfg();
  sbConfig.set_viterations( 40 );
  sbConfig.set_piterations( 40 );

  // Soft-soft and soft-rigid collisions
  sbConfig.set_collisions( 0x11 );

  // Friction
  sbConfig.set_kDF( 0.9 );
  // Damping
  sbConfig.set_kDP( 0.01 );
  // Pressure
  sbConfig.set_kPR( pressure );
  // Stiffness
  volumeSoftBody.get_m_materials().at( 0 ).set_m_kLST( 0.9 );
  volumeSoftBody.get_m_materials().at( 0 ).set_m_kAST( 0.9 );
  


  volumeSoftBody.setTotalMass( mass, false )
  Ammo.castObject( volumeSoftBody, Ammo.btCollisionObject ).getCollisionShape().setMargin( margin );
  physicsWorld.addSoftBody( volumeSoftBody, 1, -1 );
  volume.userData.physicsBody = volumeSoftBody;
  // Disable deactivation
  volumeSoftBody.setActivationState( 4 );
  
  volumeSoftBody.getCollisionShape().setMargin( 0.05 );
  volumeSoftBody.setTotalMass( 5 );
  
  softBodies.push( volume );

}









			function createParalellepiped( sx, sy, sz, mass, pos, quat, material ) {
				const threeObject = new THREE.Mesh( new THREE.BoxGeometry( sx, sy, sz, 1, 1, 1 ), material );
				const shape = new Ammo.btBoxShape( new Ammo.btVector3( sx * 0.5, sy * 0.5, sz * 0.5 ) );
				shape.setMargin( margin );
				createRigidBody( threeObject, shape, mass, pos, quat );
				return threeObject;
			}




 
   
 // прослушиватели событий клавиатуры:
	keyStates = {};
	document.addEventListener( 'keydown', ( event ) => {	
		keyStates[ event.code ] = true;  
	}, false );

	document.addEventListener( 'keyup', ( event ) => {
		keyStates[ event.code ] = false;
	}, false );
  
  
	function movingfunction() { 
      // numpad 13 52  
      if ( keyStates[ 'Numpad4' ] ) {
    console.log('d')
      }
      
      if ( keyStates[ 'Numpad5' ] ) {
      }

	}





//

			function createRigidBody( threeObject, physicsShape, mass, pos, quat ) {

				threeObject.position.copy( pos );
				threeObject.quaternion.copy( quat );

				const transform = new Ammo.btTransform();
				transform.setIdentity();
				transform.setOrigin( new Ammo.btVector3( pos.x, pos.y, pos.z ) );
				transform.setRotation( new Ammo.btQuaternion( quat.x, quat.y, quat.z, quat.w ) );
				const motionState = new Ammo.btDefaultMotionState( transform );

				const localInertia = new Ammo.btVector3( 0, 0, 0 );
				physicsShape.calculateLocalInertia( mass, localInertia );

				const rbInfo = new Ammo.btRigidBodyConstructionInfo( mass, motionState, physicsShape, localInertia );
				const body = new Ammo.btRigidBody( rbInfo );

				threeObject.userData.physicsBody = body;

				scene.add( threeObject );

				if ( mass > 0 ) {

					rigidBodies.push( threeObject );

					// Disable deactivation
					body.setActivationState( 4 );

				}
				physicsWorld.addRigidBody( body );
				return body;
			}


			function initInput() {
				window.addEventListener( 'pointerdown', function ( event ) {
					if ( ! clickRequest ) {
						mouseCoords.set(
							( event.clientX / window.innerWidth ) * 2 - 1,
							- ( event.clientY / window.innerHeight ) * 2 + 1
						);

						clickRequest = true;
					}
				} );

			}

			function processClick() {
      
				if ( clickRequest ) {
					raycaster.setFromCamera( mouseCoords, camera );

					// Creates a ball
					const ballMass = 3;
					const ballRadius = 0.4;

					const ball = new THREE.Mesh( new THREE.SphereGeometry( ballRadius, 18, 16 ), ballMaterial );
					ball.castShadow = true;
					ball.receiveShadow = true;
					const ballShape = new Ammo.btSphereShape( ballRadius );
					ballShape.setMargin( margin );
					pos.copy( raycaster.ray.direction );
					pos.add( raycaster.ray.origin );
					quat.set( 0, 0, 0, 1 );
					const ballBody = createRigidBody( ball, ballShape, ballMass, pos, quat );
					ballBody.setFriction( 0.5 );

					pos.copy( raycaster.ray.direction );
					pos.multiplyScalar( 14 );
					ballBody.setLinearVelocity( new Ammo.btVector3( pos.x, pos.y, pos.z ) );

					clickRequest = false;

				}

			}

		

			function updatePhysics( deltaTime ) {

				// Step world
				physicsWorld.stepSimulation( deltaTime, 10 );

				// Update soft volumes
				for ( let i = 0, il = softBodies.length; i < il; i ++ ) {

					const volume = softBodies[ i ];
					const geometry = volume.geometry;
					const softBody = volume.userData.physicsBody;
					const volumePositions = geometry.attributes.position.array;
					const volumeNormals = geometry.attributes.normal.array;
					const association = geometry.ammoIndexAssociation;
					const numVerts = association.length;
					const nodes = softBody.get_m_nodes();
					for ( let j = 0; j < numVerts; j ++ ) {

						const node = nodes.at( j );
						const nodePos = node.get_m_x();
						const x = nodePos.x();
						const y = nodePos.y();
						const z = nodePos.z();
						const nodeNormal = node.get_m_n();
						const nx = nodeNormal.x();
						const ny = nodeNormal.y();
						const nz = nodeNormal.z();

						const assocVertex = association[ j ];

						for ( let k = 0, kl = assocVertex.length; k < kl; k ++ ) {

							let indexVertex = assocVertex[ k ];
							volumePositions[ indexVertex ] = x;
							volumeNormals[ indexVertex ] = nx;
							indexVertex ++;
							volumePositions[ indexVertex ] = y;
							volumeNormals[ indexVertex ] = ny;
							indexVertex ++;
							volumePositions[ indexVertex ] = z;
							volumeNormals[ indexVertex ] = nz;

						}

					}

					geometry.attributes.position.needsUpdate = true;
					geometry.attributes.normal.needsUpdate = true;

				}

				// Update rigid bodies
				for ( let i = 0, il = rigidBodies.length; i < il; i ++ ) {

					const objThree = rigidBodies[ i ];
					const objPhys = objThree.userData.physicsBody;
					const ms = objPhys.getMotionState();
					if ( ms ) {

						ms.getWorldTransform( transformAux1 );
						const p = transformAux1.getOrigin();
						const q = transformAux1.getRotation();
						objThree.position.set( p.x(), p.y(), p.z() );
						objThree.quaternion.set( q.x(), q.y(), q.z(), q.w() );

					}
				}
			}
      
      
     function onWindowResize() {
				camera.aspect = window.innerWidth / window.innerHeight;
				camera.updateProjectionMatrix();
				renderer.setSize( window.innerWidth, window.innerHeight );
			}

			function animate() {
				requestAnimationFrame( animate );
				render();
        movingfunction();
				stats.update();
			}

			function render() {
				const deltaTime = clock.getDelta();
				updatePhysics( deltaTime );
				processClick();
				renderer.render( scene, camera );
			}